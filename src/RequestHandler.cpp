/**
 * Module for handling requests, via invoking appropriate libcernvm methods.
 * Author: Petr Jirout, 2016
 */

#include <algorithm>
#include <iostream>
#include <utility>
#include <map>
#include <vector>
#ifndef _WIN32
#include <unistd.h> // for exec
#endif

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include <CernVM/Hypervisor.h>
#include <CernVM/ParameterMap.h>
#include <CernVM/ProgressFeedback.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxCommon.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxSession.h>

#include "RequestHandler.h"


using namespace Launch;


//helper functions and definitions in an anonymous namespace (local)
namespace {

typedef Tools::configMapType                paramMapType;
typedef std::map<std::string, HVSessionPtr> sessionMapType;

//How many times we try to destroy the VM (must be positive)
const int DESTROY_TRIES = 2;

//If no parameters are provided (either via user param file or global config), these are used
const paramMapType DefaultCreationParams = {
    {"apiPort", "22"},
    {"cernvmVersion", "latest"},
    {"cpus", "1"},
    {"memory", "2048"},
    {"disk", "20000"},
    {"executionCap", "100"},
    {"flags", "49"}, // 64bit, headful mode, graphical extensions
};

//Fields to print while creating a machine
const std::vector<std::string> CreationInfoFields = {
    "name",
    "cpus",
    "memory",
    "disk",
    "cernvmVersion",
    "sharedFolder",
};


//Check if the params have all the required params, print error message and return false if not
bool CheckCreationParameters(ParameterMapPtr params);
std::string  PromptForMachineName(const std::string& defaultValue);
HVSessionPtr FindSessionByName(const std::string& machineName, HVInstancePtr& hypervisor, bool loadSessions=false);

} //anonymous namespace


//-----------------------------------------------------------------------------
// RequestHandler class
//-----------------------------------------------------------------------------

bool RequestHandler::listCvmMachines() {
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    //load previously stored sessions
    hv->loadSessions();
    sessionMapType sessions = hv->sessions;

    for(sessionMapType::iterator it=sessions.begin(); it != sessions.end(); ++it) {
        HVSessionPtr session = it->second;

        std::string name = session->parameters->get("name", "");
        std::string cvmVersion = session->parameters->get("cernvmVersion", "");
        std::string apiPort = session->local->get("apiPort", "");

        if (!name.empty() && !cvmVersion.empty())
            std::cout << name << ":\tCVM: " << cvmVersion << "\tport: " << apiPort << std::endl;
    }

    return true;
}


bool RequestHandler::listRunningCvmMachines() {
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    //load previously stored sessions
    hv->loadSessions();

    sessionMapType sessions = hv->sessions;
    if (sessions.size() == 0) //we have no our sessions
        return true;

    std::vector<std::string> runningVms = hv->getRunningMachines();

    for(sessionMapType::iterator it=sessions.begin(); it != sessions.end(); ++it) {
        HVSessionPtr session = it->second;

        std::string name = session->parameters->get("name", "");
        std::string cvmVersion = session->parameters->get("cernvmVersion", "");
        std::string apiPort = session->local->get("apiPort", "");

        if (!name.empty() && !cvmVersion.empty()
                && std::find(runningVms.begin(), runningVms.end(), name) != runningVms.end()) {
            //we've got a CVM machine, which is running
            std::cout << name << ":\tCVM: " << cvmVersion << "\tport: " << apiPort << std::endl;
        }
    }

    return true;
}


bool RequestHandler::isMachineRunning(const std::string& machineName) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    //load previously stored sessions
    hv->loadSessions();

    sessionMapType sessions = hv->sessions;
    if (sessions.size() == 0) //we have no our sessions
        return false;

    std::vector<std::string> runningVms = hv->getRunningMachines();
    for(sessionMapType::iterator it=sessions.begin(); it != sessions.end(); ++it) {
        HVSessionPtr session = it->second;

        std::string name = session->parameters->get("name", "");
        if (machineName != name)
            continue;

        //we got our machine
        if (std::find(runningVms.begin(), runningVms.end(), name) != runningVms.end())
            return true;
    }

    return false; //we didn't match it
}


bool RequestHandler::listMachineDetail(const std::string& machineName) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    hv->loadSessions();

    HVSessionPtr session = hv->sessionByName(machineName);
    if (!session) {
        std::cerr << "Unable to find the machine: " << machineName << std::endl;
        return false; //we didn't match the name
    }

    //get header information
    std::string name = session->parameters->get("name", "");
    std::string cvmVersion = session->parameters->get("cernvmVersion", "");

    if (!name.empty() && !cvmVersion.empty()) //we've got a CVM machine
        std::cout << name << ":\tCVM: " << cvmVersion << std::endl;

    const std::vector<std::string> parametersFields = {
        "cpus",
        "memory",
        "disk",
        "executionCap",
        "cernvmVersion",
    };

    const std::vector<std::string> localFields = {
        "baseFolder",
    };

    Tools::PrintParameters(parametersFields, session->parameters);
    Tools::PrintParameters(localFields, session->local);

    //now print the forwarding part
    std::string localApiPort = session->local->get("apiPort", "");
    std::string paramApiPort = session->parameters->get("apiPort", "");
    std::cout << "\tforwarded ports: " << paramApiPort << " (VM) --> " << localApiPort << " (localhost)" << std::endl;

    return true;
}


bool RequestHandler::createMachine(const std::string& userDataFile, bool startMachine, Tools::configMapType& paramMap) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    Tools::configMapType::iterator it;

    if (userDataFile.empty()) { // no user data provided, ask to use the default
        std::string decision;
        std::cout << "You have not provided a user data file, do you want to use a default one?\n";
        std::cout << "Default user data:\n\n" << DEFAULT_USER_DATA << std::endl;
        std::cout << "Continue with default context? [Y/n]: "; //default is yes
        bool gotInput = Tools::GetUserInput(decision);
        boost::algorithm::to_lower(decision);

        if (gotInput && decision != "y" && decision != "yes") { //something else than yes
            std::cout << "Aborting, no context provided\n";
            return false;
        }
        //Save user data
        paramMap.insert(std::make_pair<const std::string, const std::string>("userData", static_cast<const std::string>(DEFAULT_USER_DATA)));
    }
    else { //user wants to provide the user data
        std::string userData;
        bool res = Tools::LoadFileIntoString(userDataFile, userData);

        if (!res) {
            std::cerr << "Error while processing file: " << userDataFile << std::endl;
            return false;
        }
        //if user accidentally specified userData in parameter map file, we overwrite it
        it = paramMap.find("userData");
        if (it != paramMap.end()) {
            std::cout << "Ignoring the userData specified in the parameter file, using userData file instead\n";
            paramMap.erase(it);
        }
        //Save user data
        paramMap.insert(std::make_pair<const std::string, const std::string>("userData", static_cast<const std::string>(userData)));
    }

    //Load missing values from the global config file
    Tools::configMapTypePtr configMap = Tools::GetGlobalConfig();
    if (configMap)
        Tools::AddMissingValuesToMap(paramMap, *configMap);

    //Load missing values from the hardcoded config
    Tools::AddMissingValuesToMap(paramMap, DefaultCreationParams);

    //If user wants to create the machine from his own ISO, we need to let libcernvm know
    if (paramMap.find("isoPath") != paramMap.end()) {
        std::string isoPath = paramMap.at("isoPath");
        if (! file_exists(isoPath)) {
            std::cerr << "Provided ISO path '" << isoPath << "' does not exist or is not readable\n";
            return false;
        }

        //Set the import flag
        std::string flags;
        it = paramMap.find("flags");
        if (it == paramMap.end())
            flags = "49";
        else {
            flags = it->second;
            paramMap.erase(it);
        }

        Tools::SetFlagsInString(flags, HVF_DEPLOYMENT_ISO_LOCAL);
        paramMap.insert(std::make_pair("flags", flags));

        //We don't know what CernVM ISO version user provided, so we just set the cernvmVersion to the given path
        paramMap.erase(paramMap.find("cernvmVersion"));
        paramMap.insert(std::make_pair("cernvmVersion", isoPath));
    }

    //Convert the parameter map from std::map
    ParameterMapPtr parameters = ParameterMap::instance();
    parameters->fromMap(&paramMap);

    if (!CheckCreationParameters(parameters))
        return false; // user forgot to specify some parameters

    //The same machine can already have a session, check it
    hv->loadSessions();
    sessionMapType sessions = hv->sessions;

    std::string machineName = parameters->get("name", "");

    //VM name missing, prompt the user
    if (machineName.empty()) {
        std::string defaultMachineName = getFilename(userDataFile); //get the basename
        if (defaultMachineName.find('.') != std::string::npos) //strip extension if needed
            defaultMachineName = defaultMachineName.substr(0, defaultMachineName.find('.'));
        if (defaultMachineName.empty())
            defaultMachineName = "CernVM";

        machineName = PromptForMachineName(defaultMachineName);
        parameters->set("name", machineName);
    }

    if (! isSanitized(&machineName, SAFE_ALNUM_CHARS)) {
        std::cerr << "Machine name contains illegal characters, use only following: " << SAFE_ALNUM_CHARS << std::endl;
        return false;
    }

    if (FindSessionByName(machineName, hv)) { //we already have this session
        std::cerr << "The machine already exists\n";
        return false;
    }

    //allocate a new session
    HVSessionPtr session = hv->allocateSession();

    //load our parameters into the newly created session
    session->parameters->fromParameters(parameters, false, true); //don't clear defaults, but overwrite local keys
    session->wait();

    //get our newly allocated session and open it (i.e. start the FSM => initiate the creation)
    session = FindSessionByName(machineName, hv);
    if (!session) {
        std::cerr << "Could not open the session\n";
        return false;
    }

    //we need to start the session, so the creation process gets initiated
    ParameterMapPtr emptyMap = ParameterMap::instance(); //we don't want to specify additional parameters
    session->start(emptyMap); //start scheduled
    session->wait(); //wait for the session until it finishes all tasks

    std::cout << "Parameters used for the machine creation:\n";
    Tools::PrintParameters(CreationInfoFields, session->parameters);

    if (!startMachine) { //stop the session if required
        session->stop();
        session->wait();
    }

    return true;
}


bool RequestHandler::importMachine(const std::string& imageFilename, bool startMachine, Tools::configMapType& paramMap) {
    //set all the required information for the libcernvm
    //set the ovaImport flag, so libcernvm knows we're making OVA import
    paramMap.insert(std::make_pair("ovaImport", "true"));

    //made path canonical and save it
    boost::filesystem::path imageFilePath;
    try {
        imageFilePath = boost::filesystem::canonical(imageFilename);
    }
    catch (boost::filesystem::filesystem_error& e) {
        std::string errStr = e.what();
        std::cerr << errStr.substr(errStr.find(": ")+2) << std::endl; // strip 'boost::filesystem::canonical: '
        return false;
    }

    Tools::configMapType::iterator it = paramMap.find("ovaPath");
    if (it != paramMap.end())
        paramMap.erase(it);
    paramMap.insert(std::make_pair("ovaPath", imageFilePath.string()));

    //set the import flag
    std::string flags;
    it = paramMap.find("flags");
    if (it == paramMap.end())
        flags = "49";
    else {
        flags = it->second;
        paramMap.erase(it);
    }

    Tools::SetFlagsInString(flags, HVF_IMPORT_OVA);

    paramMap.insert(std::make_pair("flags", flags));

    return this->createMachine("", startMachine, paramMap); // no user data file
}


bool RequestHandler::destroyMachine(const std::string& machineName, bool force) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    HVSessionPtr session = FindSessionByName(machineName, hv, true);
    if (!session) {
        std::cerr << "Unable to find the machine: " << machineName << std::endl;
        return false; //we didn't match the name
    }

    VBoxSessionPtr vboxSession = boost::static_pointer_cast<VBoxSession>(session);
    if (!vboxSession) {
        std::cerr << "This machine is not managed by VirtualBox, cannot delete it\n";
        return false;
    }

    if (this->isMachineRunning(machineName)) {
        if (!force) { //prompt user for confirmation
            std::cout << "The machine '" << machineName << "' is running, do you want do destroy it? [y/N]: ";
            std::string decision;
            bool gotInput = Tools::GetUserInput(decision);
            boost::algorithm::to_lower(decision);

            if (!gotInput || (decision != "y" && decision != "yes")) { //just <Enter> or something else than yes
                return true; //user does not want to destroy it
            }
        }
        vboxSession->stop();
        vboxSession->wait();
    }

    int ret;
    for (int i=0; i < DESTROY_TRIES; ++i) {
        ret = vboxSession->destroyVM();
        vboxSession->wait();

        if (ret == HVE_OK)
            break;

        sleepMs(6000); //give VBox time to recover
    }
    if (ret != HVE_OK) { // we failed every time
        std::cerr << "Unable to delete the machine, tried " << DESTROY_TRIES << " times\n";
        return false;
    }
    hv->sessionDelete(session);

    return true;
}


bool RequestHandler::pauseMachine(const std::string& machineName) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    HVSessionPtr session = FindSessionByName(machineName, hv, true);
    if (!session) {
        std::cerr << "Unable to find the machine: " << machineName << std::endl;
        return false; //we didn't match the name
    }

    session->pause();
    session->wait(); //wait for the session until it finishes all tasks

    return true; //we started the session, we don't have to go through the rest of machines
}


bool RequestHandler::sshIntoMachine(const std::string& login) {
#ifdef _WIN32
    std::cerr << "SSH into machine is not supported on Windows\n";
    return false;
#else // linux or mac
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }
    hv->loadSessions();

    std::string machineName = login;
    std::string username;
    std::vector<std::string> tokens = Tools::SplitString(login, '@', 2);
    if (tokens.size() == 2) {
        machineName = tokens[1];
        username = tokens[0];
    }

    HVSessionPtr session = hv->sessionByName(machineName);
    if (!session) {
        std::cerr << "Unable to find the machine: " << machineName << std::endl;
        return false; //we didn't match the name
    }

    if (! this->isMachineRunning(machineName)) {
        std::cerr << "Machine '" << machineName << "' is not running\n";
        return false;
    }

    //Prompt username
    if (username.empty()) {
        std::cout << "Username: ";
        if (!Tools::GetUserInput(username)) {
            std::cerr << "Username is mandatory, exiting";
            return false;
        }
    }

    //Exec should look like this: ssh -p PORT_NUM USER@127.0.0.1

    std::string sshBin = which("ssh"); // goes through PATH env
    if (sshBin.empty()) {
        std::cerr << "Unable to locate the SSH binary\n";
        return false;
    }

    std::string port = session->local->get("apiPort", "");
    if (port.empty()) {
        std::cerr << "No ssh port found for this machine\n";
        return false;
    }
    std::string x11String = "-Y";
    std::string portString = "-p " + port;
    std::string fullAddress = username + "@127.0.0.1";

    int res = execl(sshBin.c_str(), sshBin.c_str(), x11String.c_str(), portString.c_str(), fullAddress.c_str(),
                    (char*) NULL);
    if (res == -1) {
        std::cerr << "Unable to launch ssh\n";
        return false;
    }

    return true; // exec replaces this process, so we should never return
#endif // linux or mac
}


bool RequestHandler::startMachine(const std::string& machineName) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    HVSessionPtr session = FindSessionByName(machineName, hv, true);
    if (!session) {
        std::cerr << "Unable to find the machine: " << machineName << std::endl;
        return false; //we didn't match the name
    }

    ParameterMapPtr emptyMap = ParameterMap::instance(); //we don't want to specify additional parameters
    session->start(emptyMap);

    session->wait(); //wait for the session until it finishes all tasks

    return true; //we started the session, we don't have to go through the rest of machines
}


bool RequestHandler::stopMachine(const std::string& machineName) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    HVSessionPtr session = FindSessionByName(machineName, hv, true);
    if (!session)
        return false; //cannot open the session

    session->hibernate(); //save state and stop

    session->wait(); //wait for the session until it finishes all tasks

    return true; //we started the session, we don't have to go through the rest of machines
}

//-----------------------------------------------------------------------------
// Local helper functions
//-----------------------------------------------------------------------------
namespace {

bool CheckCreationParameters(ParameterMapPtr params) {
    //this is needed by libcernvm
    params->set("secret", "defaultSecret");

    //check flags
    int flags = params->getNum<int>("flags", 0);
    if (flags) {
        if (flags & HVF_DEPLOYMENT_HDD_LOCAL) {
            //we need diskPath
            if ((params->get("diskPath", "")).empty()) {
                std::cerr << "You need to provide 'diskPath' parameter for deployment from a local file\n";
                return false;
            }
        }
        if (flags & HVF_DEPLOYMENT_ISO_LOCAL) {
            //we need isoPath
            if ((params->get("isoPath", "")).empty()) {
                std::cerr << "You need to provide 'isoPath' parameter for deployment from a local ISO file\n";
                return false;
            }
        }
        else if (flags & HVF_DEPLOYMENT_HDD) {
            //we need diskURL and diskChecksum
            if ((params->get("diskURL", "")).empty() || (params->get("diskChecksum", "")).empty()) {
                std::cerr << "You need to provide 'diskURL' and 'diskChecksum' parameters for online deployment\n";
                return false;
            }
        }
    }
    //check if paths are canonical
    std::vector<std::string> paths = {
        "sharedFolder",
        "diskPath",
    };
    for (std::vector<std::string>::iterator it = paths.begin(); it != paths.end(); ++it) {
        std::string value = params->get(*it, "");
        if ((! value.empty() ) && (! Tools::IsCanonicalPath(value) )) {
            std::cerr << "Value for parameter '" << *it << "' is not a canonical path: '" << value << "'" <<std::endl;
            return false;
        }
    }

    return true;
}


//Prompt for username. if none is provided, use given default
std::string PromptForMachineName(const std::string& defaultValue) {
    std::cout << "Enter VM name [" << defaultValue << "]: ";
    std::string userValue;
    if (! Tools::GetUserInput(userValue)) //no input
        return defaultValue;

    return userValue;
}


//Find and opens a session with the corresponding machineName. If 'loadSession' flag
//is true, we load sessions on the hypervisor. Defaults to false.
HVSessionPtr FindSessionByName(const std::string& machineName, HVInstancePtr& hypervisor, bool loadSessions) {
    if (!hypervisor)
        return HVSessionPtr();

    if (loadSessions)
        hypervisor->loadSessions();

    HVSessionPtr session = hypervisor->sessionByName(machineName);
    if (!session)
        return HVSessionPtr();

    //we found our session, try to open it
    ParameterMapPtr sessParamMap = session->parameters;
    FiniteTaskPtr pOpen = boost::make_shared<FiniteTask>();
    pOpen->setMax(1);

    //open the session, i.e. start the FSM thread
    session = hypervisor->sessionOpen(sessParamMap, pOpen, false); //bypass verification, we're locals
    if (!session)
        return HVSessionPtr();
    session->wait();

    return session;
}


} //anonymous namespace

