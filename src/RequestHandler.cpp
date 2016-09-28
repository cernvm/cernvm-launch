/**
 * Module for handling requests, via invoking appropriate libcernvm methods.
 * Author: Petr Jirout, 2016
 */

#include <algorithm>
#include <iostream>
#include <utility>
#include <map>
#include <vector>

#include <boost/shared_ptr.hpp>

#include <CernVM/Hypervisor.h>
#include <CernVM/ParameterMap.h>
#include <CernVM/ProgressFeedback.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxCommon.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxSession.h>

#include "Tools.h"
#include "RequestHandler.h"


using namespace Launch;


//helper functions and definitions in an anonymous namespace (local)
namespace {

typedef Tools::configMapType  paramMapType;
typedef std::map<std::string, HVSessionPtr>             sessionMapType;

//If no parameters are provided (either via user param file or global config), these are used
const paramMapType DefaultCreationParams = {
    {"apiPort", "22"},
    {"cernvmVersion", "2.6.9"},
    {"cpus", "1"},
    {"memory", "512"},
    {"disk", "5000"},
    {"executionCap", "100"},
    {"flags", "49"}, // 64bit, headful mode, graphical extensions
};

//Fields to print while creating a machine
const std::vector<std::string> CreationInfoFields = {
    "name",
    "cpus",
    "memory",
    "disk",
    "executionCap",
    "cernvmVersion",
    "apiPort",
//    "sharedFolder", //TODO add
};


bool         CheckMandatoryParameters(const ParameterMapPtr& parameters, std::string& missingParameter);
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
    std::string apiPort = session->local->get("apiPort", "");

    if (!name.empty() && !cvmVersion.empty()) //we've got a CVM machine
        std::cout << name << ":\tCVM: " << cvmVersion << "\tport: " << apiPort << std::endl;

    const std::vector<std::string> parametersFields = {
        "cpus",
        "memory"
    };

    const std::vector<std::string> localFields = {
        "baseFolder",
        "rdpPort"
    };

    Tools::PrintParameters(parametersFields, session->parameters);
    Tools::PrintParameters(localFields, session->local);
    //PrintParameters(machineFields, session->machine);

    return true;
}


bool RequestHandler::createMachine(const std::string& userDataFile, bool startMachine, const std::string& parameterMapFile) {
    paramMapType paramMap;

    std::string userData;
    bool res = Tools::LoadFileIntoString(userDataFile, userData);

    if (!res) {
        std::cerr << "Error while processing file: " << userDataFile << std::endl;
        return false;
    }

    if (! parameterMapFile.empty()) {
        res = Tools::LoadFileIntoMap(parameterMapFile, paramMap);
        if (!res) {
            std::cerr << "Error while processing file: " << parameterMapFile << std::endl;
            return false;
        }
    }

    //if user accidentally specified userData in parameter map file, we overwrite it
    paramMapType::iterator it = paramMap.find("userData");
    if (it != paramMap.end()) {
        std::cout << "Ignoring the userData specified in the parameter file, using userData file instead\n";
        paramMap.erase(it);
    }

    paramMap.insert(std::make_pair<const std::string, const std::string>("userData", static_cast<const std::string>(userData)));

    //Load missing values from the global config file
    Tools::configMapTypePtr configMap = Tools::GetGlobalConfig();
    if (configMap) {
        Tools::AddMissingValuesToMap(paramMap, *configMap);
    }

    //Load missing values from the hardcoded config
    Tools::AddMissingValuesToMap(paramMap, DefaultCreationParams);

    HVInstancePtr hv = detectHypervisor();
    if (!hv) {
        std::cerr << "Unable to detect hypervisor\n";
        return false;
    }

    //create a parameter map from std::map
    ParameterMapPtr parameters = ParameterMap::instance();
    parameters->fromMap(&paramMap);

    //The same machine can already have a session, check it
    hv->loadSessions();
    sessionMapType sessions = hv->sessions;

    std::string missingParameter;
    if (!CheckMandatoryParameters(parameters, missingParameter)) {
        std::cerr << "Cannot create a virtual machine, missing parameter: " << missingParameter << std::endl;
        return false;
    }

    std::string machineName = parameters->get("name");

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
    session->start(emptyMap);
    session->wait(); //wait for the session until it finishes all tasks

    //TODO check start return value?  ^
    std::cout << "Parameters used for the machine creation:\n";
    Tools::PrintParameters(CreationInfoFields, session->parameters);

    if (!startMachine) { //stop the session if required
        session->stop();
        session->wait();
    }

    return true;
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

    int ret = vboxSession->destroyVM();
    vboxSession->wait();

    if (ret != HVE_OK) {
        if (force) { //machine is probably running, stop it and try again
            vboxSession->stop();
            vboxSession->wait();
            ret = vboxSession->destroyVM();
            vboxSession->wait();
            if (ret != HVE_OK) {
                std::cerr << "Unable to delete the machine.\n";
                return false;
            }
        }
        else {
            std::cerr << "Unable to delete the machine, probably it's running. Use '--force' to delete anyway.\n";
            return false;
        }
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

//Check mandatory parameters for a VM creation. If the 'secret' param is not present, we add
//a default one (it's required for the sessionOpen)
//Return false if any is missing.
bool CheckMandatoryParameters(const ParameterMapPtr& parameters, std::string& missingParameter) {
    std::vector<std::string> mandatoryParams = {
        "name",
    };

    for (std::vector<std::string>::iterator it = mandatoryParams.begin(); it != mandatoryParams.end(); ++it) {
        if (parameters->get(*it, "").empty()) {
            missingParameter = *it;
            return false;
        }
    }

    if (parameters->get("secret", "").empty())
        parameters->set("secret", "defaultSecret");

    return true;
}


//Find and opens a session with the corresponding machineName. If 'loadSession' flag
//is true, we load sessions on the hypervisor. Defaults to false.
HVSessionPtr FindSessionByName(const std::string& machineName, HVInstancePtr& hypervisor, bool loadSessions) {
    if (!hypervisor)
        return NULL;

    if (loadSessions)
        hypervisor->loadSessions();

    HVSessionPtr session = hypervisor->sessionByName(machineName);
    if (!session)
        return NULL;

    //we found our session, try to open it
    ParameterMapPtr sessParamMap = session->parameters;
    FiniteTaskPtr pOpen = boost::make_shared<FiniteTask>();
    pOpen->setMax(1);

    //open the session, i.e. start the FSM thread
    session = hypervisor->sessionOpen(sessParamMap, pOpen, false); //bypass verification, we're locals
    session->wait();

    return session;
}


} //anonymous namespace

