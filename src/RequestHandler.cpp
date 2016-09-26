#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include <CernVM/Hypervisor.h>
#include <CernVM/ParameterMap.h>
#include <CernVM/ProgressFeedback.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxCommon.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxSession.h>

#include "RequestHandler.h"


#define KEY_VALUE_SEPARATOR '='
#define COMMENT_CHAR        '#'


//helper functions and definitions in an anonymous namespace
namespace {

bool         CheckMandatoryParameters(const ParameterMapPtr& parameters, std::string& missingParameter);
HVSessionPtr FindSessionByName(const std::string& machineName, HVInstancePtr& hypervisor, bool loadSessions=false);
bool         LoadFileIntoMap(const std::string& filename, std::map<const std::string, const std::string>& outMap);
bool         LoadFileIntoString(const std::string& filename, std::string& output);
void         PrintParameters(const std::vector<std::string>& fields, ParameterMapPtr paramMap);

typedef std::map<const std::string, const std::string>  paramMapType;
typedef std::map<std::string, HVSessionPtr>             sessionMapType;

}


//-----------------------------------------------------------------------------
// RequestHandler class
//-----------------------------------------------------------------------------

bool Launch::RequestHandler::listCvmMachines() {
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


bool Launch::RequestHandler::listRunningCvmMachines() {
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


bool Launch::RequestHandler::listMachineDetail(const std::string& machineName) {
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

    PrintParameters(parametersFields, session->parameters);
    PrintParameters(localFields, session->local);
    //PrintParameters(machineFields, session->machine);

    return true;
}


bool Launch::RequestHandler::createMachine(const std::string& parameterMapFile, const std::string& userDataFile, bool startMachine) {
    std::map<const std::string, const std::string> paramMap;
    bool res = LoadFileIntoMap(parameterMapFile, paramMap);

    if (!res) {
        std::cerr << "Error while processing file: " << parameterMapFile << std::endl;
        return false;
    }

    std::string userData;
    res = LoadFileIntoString(userDataFile, userData);

    if (!res) {
        std::cerr << "Error while processing file: " << userDataFile << std::endl;
        return false;
    }

    //if user accidentally specified userData in parameter map file, we overwrite it
    std::map<const std::string, const std::string>::iterator it = paramMap.find("userData");
    if (it != paramMap.end()) {
        std::cout << "Ignoring the userData specified in the parameter file, using userData file instead\n";
        paramMap.erase(it);
    }

    paramMap.insert(std::make_pair<const std::string, const std::string>("userData", static_cast<const std::string>(userData)));

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

    if (!startMachine) { //stop the session if required
        session->stop();
        session->wait();
    }

    return true;
}


bool Launch::RequestHandler::destroyMachine(const std::string& machineName) {
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

    vboxSession->DestroyVM();
    vboxSession->wait();

    hv->sessionDelete(session);

    return true;
}


bool Launch::RequestHandler::pauseMachine(const std::string& machineName) {
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


bool Launch::RequestHandler::startMachine(const std::string& machineName) {
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


bool Launch::RequestHandler::stopMachine(const std::string& machineName) {
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

//Checks mandatory parameters for a VM creation.
//Returns false if any is missing.
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


//Load <string,string> map from a file with key=value items.
//Lines starting with '#' are considered as comments, thus ignored.
//Lines without KEY_VALUE_SEPARATOR (i.e. '=') are ignored as well
//We do not store items with an empty value.
bool LoadFileIntoMap(const std::string& filename, std::map<const std::string, const std::string>& outMap) {
    std::ifstream ifs (filename);

    if (!ifs.good()) //error when opening a file
        return false;

    for (std::string line; std::getline(ifs, line); ) { //for each line in the input
        std::string key, value;
        size_t len = line.size();
        size_t i = 0;

        //ignore comments
        if (len == 0 || line[0] == COMMENT_CHAR)
            continue;

        while (i < len && line[i++] != KEY_VALUE_SEPARATOR) //read the key
            key.push_back(line[i-1]);

        while (i++ < len) //read the value
            value.push_back(line[i-1]);

        boost::trim(key);
        boost::trim(value);
        if (key.empty() || value.empty()) //ignore invalid lines: empty or without value
            continue;
        else { //put on the map (little cumbersome, since we have a const std::string map)
            std::map<const std::string, const std::string>::iterator it = outMap.find(key);
            if (it != outMap.end()) //the key already exists, erase it
                outMap.erase(it);
            outMap.insert(std::make_pair(key, value));
        }
    }
    return true;
}

//Fill output string with the content of the given file.
//If an error occurs, false is returned.
bool LoadFileIntoString(const std::string& filename, std::string& output) {
    std::ifstream ifs (filename);

    if (!ifs.good()) //error when opening a file
        return false;

    for (std::string line; std::getline(ifs, line); ) { //for each line in the input
        output += line;
        output += "\n"; //std::getline discards the newline
    }

    return true;
}

//Print specified items from the given parameter map
void PrintParameters(const std::vector<std::string>& fields, ParameterMapPtr paramMap) {
    std::vector<std::string>::const_iterator it = fields.begin();
    for (; it != fields.end(); ++it) {
        std::string value = paramMap->get(*it, "");
        if (! value.empty())
            std::cout << "\t" << *it << ": " << value << std::endl;
    }
}

} //anonymous namespace

