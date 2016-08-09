#include <iostream>
#include <map>
#include <string>
#include <utility>

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


//helper functions and definitions
namespace {

bool CheckMandatoryParameters(const ParameterMapPtr& parameters);
HVSessionPtr FindSessionByName(const std::string& machineName, HVInstancePtr& hypervisor, bool loadSessions=false);
bool LoadMapFromFile(const std::string& filename, std::map<const std::string, const std::string>& outMap);
bool LoadFileIntoString(const std::string& filename, std::string& output);

typedef std::map<const std::string, const std::string>  paramMapType;
typedef std::map<std::string, HVSessionPtr>             sessionMapType;

}

//TODO
//  - add error messages printage on returns

//-----------------------------------------------------------------------------
// RequestHandler class
//-----------------------------------------------------------------------------

bool Launch::RequestHandler::listCvmMachines() {
    HVInstancePtr hv = detectHypervisor();
    if (!hv)
        return false;

    //load previously stored sessions
    hv->loadSessions();

    paramMapType paramMap;
    sessionMapType sessions = hv->sessions;

    for(sessionMapType::iterator it=sessions.begin(); it != sessions.end(); ++it) {
        HVSessionPtr session = it->second;

        //we analyze default properties (without any prefix)
        paramMap.clear();
        ParameterMapPtr sessParamMap = session->parameters;
        sessParamMap->toMap(&paramMap);

        std::string name, cvmVersion;
        for(paramMapType::iterator itt = paramMap.begin(); itt != paramMap.end(); ++itt) {
            std::string key = itt->first;
            std::string value = itt->second;

            if (key == "name")
                name = value;

            else if (key == "cernvmVersion")
                cvmVersion = value;
            //TODO IP address
            //TODO directory
        }

        if (!name.empty() && !cvmVersion.empty()) //we've got a CVM machine
            std::cout << name << "\tCVM:" << cvmVersion << std::endl;
    }

    return true;
}


bool Launch::RequestHandler::createMachine(const std::string& parameterMapFile, const std::string& userDataFile) {

    std::map<const std::string, const std::string> paramMap;
    bool res = LoadMapFromFile(parameterMapFile, paramMap);

    if (!res)
        return false;

    std::string userData;
    res = LoadFileIntoString(userDataFile, userData);

    if (!res)
        return false;

    //if user accidentally specified userData in parameter map file, we overwrite it
    std::map<const std::string, const std::string>::iterator it = paramMap.find("userData");
    if (it != paramMap.end()) {
        std::cout << "Ignoring the userData specified in the parameter file, using userData file instead\n";
        paramMap.erase(it);
    }

    paramMap.insert(std::make_pair<const std::string, const std::string>("userData", static_cast<const std::string>(userData)));


    HVInstancePtr hv = detectHypervisor();
    if (!hv)
        return false;

    //create a parameter map from std::map
    ParameterMapPtr parameters = ParameterMap::instance();
    parameters->fromMap(&paramMap);

    //The same machine can already have a session, check it
    hv->loadSessions();
    sessionMapType sessions = hv->sessions;

    if (!CheckMandatoryParameters(parameters)) {
        std::cerr << "Missing parameters, cannot create\n";
        return false;
    }

    std::string machineName = parameters->get("name");

    if (FindSessionByName(machineName, hv)) { //we already have this session
        std::cerr << "The machine already exists\n";
        return false;
    }

    //allocate a new session
    HVSessionPtr session = hv->allocateSession();

    parameters->fromMap(&paramMap);
    //mark the parameters as initialized
    session->parameters->fromParameters(parameters, false, true); //don't clear defaults, but overwrite local keys

    session->wait();

    //get our newly allocated session and open it (i.e. start the FSM => initiate the creation)
    session = FindSessionByName(machineName, hv);

    if (!session) {
        std::cerr << "Could not open the session\n";
        return false;
    }

    //TODO in order to avoid windows popup, start it in headless mode (and then change it back to the original one)

    ParameterMapPtr emptyMap = ParameterMap::instance(); //we don't want to specify additional parameters
    session->start(emptyMap);
    session->wait(); //wait for the session until it finishes all tasks

    this->stopMachine(machineName);

    return true;
}


bool Launch::RequestHandler::deleteMachine(const std::string& machineName) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv)
        return false;

    HVSessionPtr session = FindSessionByName(machineName, hv, true);
    if (!session) {
        std::cerr << "Unable to find the machine\n";
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


bool Launch::RequestHandler::startMachine(const std::string& machineName) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv)
        return false;

    HVSessionPtr session = FindSessionByName(machineName, hv, true);
    if (!session)
        return false; //we didn't match the name

    ParameterMapPtr emptyMap = ParameterMap::instance(); //we don't want to specify additional parameters
    session->start(emptyMap);

    session->wait(); //wait for the session until it finishes all tasks

    return true; //we started the session, we don't have to go through the rest of machines
}


bool Launch::RequestHandler::stopMachine(const std::string& machineName) {
    HVInstancePtr hv = detectHypervisor();
    if (!hv)
        return false;

    HVSessionPtr session = FindSessionByName(machineName, hv, true);
    if (!session)
        return false; //cannot open the session

    session->stop();

    session->wait(); //wait for the session until it finishes all tasks

    return true; //we started the session, we don't have to go through the rest of machines
}

//-----------------------------------------------------------------------------
// Local helper functions
//-----------------------------------------------------------------------------
namespace {

//Checks mandatory parameters for a VM creation.
//Returns false if any is missing.
bool CheckMandatoryParameters(const ParameterMapPtr& parameters) {
    if (parameters->get("name", "").empty())
        return false;

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
bool LoadMapFromFile(const std::string& filename, std::map<const std::string, const std::string>& outMap) {
    std::ifstream ifs (filename);

    if (!ifs.good()) //error when opening a file
        return false;

    //TODO what about unicode characters?
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
        else { //put on the map (little cumbersome, since we have const std::string map)
            std::map<const std::string, const std::string>::iterator it = outMap.find(key);
            if (it != outMap.end()) //the key already exists, erase it
                outMap.erase(it);
            outMap.insert(std::make_pair(key, value));
            std::cout << "key: |" << key << "|\tvalue: |" << value << "|\n";
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

    //TODO what about unicode characters?
    for (std::string line; std::getline(ifs, line); ) { //for each line in the input
        output += line;
        output += "\n"; //std::getline discards the newline
    }

    return true;
}

} //anonymous namespace

