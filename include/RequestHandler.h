/**
 * Module for handling requests, via invoking appropriate libcernvm methods.
 * Author: Petr Jirout, 2016
 */

#ifndef _REQUEST_HANDLER_H
#define _REQUEST_HANDLER_H

#include <string>

#include "Tools.h"

namespace Launch {

//Handles user requests, providing appropriate response.
class RequestHandler {
    public:
        //TODO document the methods
        bool listCvmMachines();
        bool listRunningCvmMachines();
        bool listMachineDetail(const std::string& machineName);
        bool createMachine(const std::string& userDataFile, bool startMachine, Tools::configMapType& params);
        bool destroyMachine(const std::string& machineName, bool force=false);
        bool pauseMachine(const std::string& machineName);
        bool startMachine(const std::string& machineName);
        bool stopMachine(const std::string& machineName);
};

} //namespace Launch

#endif //_REQUEST_HANDLER_H

