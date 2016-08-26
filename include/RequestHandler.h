#ifndef _REQUEST_HANDLER_H
#define _REQUEST_HANDLER_H

namespace Launch {

//Handles user requests, providing appropriate response.
class RequestHandler {
    public:
        bool listCvmMachines();
        bool listRunningCvmMachines();
        bool listMachineDetail(const std::string& machineName);
        bool createMachine(const std::string& parameterMapFile, const std::string& userDataFile);
        bool destroyMachine(const std::string& machineName);
        bool startMachine(const std::string& machineName);
        bool stopMachine(const std::string& machineName);
};

} //namespace Launch

#endif //_REQUEST_HANDLER_H

