/**
 * Module for handling requests, via invoking appropriate libcernvm methods.
 * Author: Petr Jirout, 2016
 */

#ifndef _REQUEST_HANDLER_H
#define _REQUEST_HANDLER_H

#include <string>

#include "Tools.h"

namespace Launch {

const std::string DEFAULT_USER_DATA = \
"[amiconfig]\n"
"plugins=cernvm\n"
"[cernvm]\n"
"auto_login=on\n"
"organisations=\n"
"repositories=\n"
"shell=/bin/bash\n"
"config_url=http://cernvm.cern.ch/config\n"
"users=user:user:password\n"
"edition=Desktop\n"
"screenRes=1280x800\n"
"keyboard=us-acentos\n"
"startXDM=on\n";

//Handles user requests, providing appropriate response.
//All of the methods return true on success, false otherwise.
class RequestHandler {
    public:
        //Check if a given machine is running
        bool isMachineRunning(const std::string& machineName);
        //List existing CernVM machines
        bool listCvmMachines();
        //List only running CernVM machines
        bool listRunningCvmMachines();
        //List details (information) about given machine
        bool listMachineDetail(const std::string& machineName);
        //Create a new VM.
        //userDataFile: contextualization file
        //startMachine: whether to start the machine after creation
        //params: parameter map with creation parameters
        bool createMachine(const std::string& userDataFile, bool startMachine, Tools::configMapType& params);
        //Import an OVA image
        bool importMachine(const std::string& imageFilename, bool startMachine, Tools::configMapType& params);
        //Destroy a machine. By default, it does not destroy a running machine, use force=true for that
        bool destroyMachine(const std::string& machineName, bool force=false);
        //Pause machine
        bool pauseMachine(const std::string& machineName);
        //SSH into machine. It find an SSH executable and replaces cernvm-launch binary
        //with this binary (execv). Does not work on Windows.
        bool sshIntoMachine(const std::string& login);
        //Start machine. The machine can be either paused or stopped
        bool startMachine(const std::string& machineName);
        //Stop machine. Saves the state, does not do a power off
        bool stopMachine(const std::string& machineName);
};

} //namespace Launch

#endif //_REQUEST_HANDLER_H

