#include <iostream>
#include <string>
#include <map>

#include <CernVM/Utilities.h>
#include <CernVM/Hypervisor.h>
#include <CernVM/ParameterMap.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxCommon.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxSession.h>

#include "RequestHandler.h"

//Module local functions
namespace {

int DispatchArguments(int argc, char** argv, Launch::RequestHandler& handler);
void PrintHelp();
bool CheckArgCount(int argc, int desiredCount, const std::string& errorMessageOnFail);

//list of error codes returned by the program
const int ERR_OK = 0; //success
const int ERR_INVALID_PARAM_COUNT = 1;
const int ERR_INVALID_OPERATION = 2;
const int ERR_RUNTIME_ERROR = 3;

}


int main(int argc, char** argv) {
    Launch::RequestHandler handler;

    int exitCode = DispatchArguments(argc, argv, handler);

    return exitCode;
}


namespace {

//Parse given arguments, verify them, and dispatch it to the correct function.
//If anything is wrong, it prints the error message and returns appropriate code.
//On success, 0 is returned.
int DispatchArguments(int argc, char** argv, Launch::RequestHandler& handler) {
    if (argc <= 1) {
        PrintHelp();
        return ERR_INVALID_PARAM_COUNT;
    }
    std::string action = argv[1];
    bool success = true;

    //list VMs
    if (action == "list")
        if (argc == 3) {
            if (std::string(argv[2]) == "--running") //list only running machines
                success = handler.listRunningCvmMachines();
            else //the user requested details of a machine
                success = handler.listMachineDetail(argv[2]);
        }
        else
            success = handler.listCvmMachines();
    //create a VM
    else if (action == "create") {
        //E.g: ./cernvm-launch create --no-start config_file userData_file
        if (argc == 5) {
            if (std::string(argv[2]) != "--no-start") {
                std::cerr << "'create' requires two arguments: configuration_file and userData_file" << std::endl;
                return ERR_INVALID_PARAM_COUNT;
            }
            success = handler.createMachine(argv[3], argv[4], false);
        }
        //E.g: ./cernvm-launch create config_file userData_file
        else if (CheckArgCount(argc, 4, "'create' requires two arguments: configuration_file and userData_file"))
            success = handler.createMachine(argv[2], argv[3], true);
        else
            return ERR_INVALID_PARAM_COUNT;
    }
    //start a VM
    else if (action == "start") {
        if (!CheckArgCount(argc, 3, "'start' requires one argument: machine name"))
            return ERR_INVALID_PARAM_COUNT;
        success = handler.startMachine(argv[2]);
    }
    //stop a VM
    else if (action == "stop") {
        if (!CheckArgCount(argc, 3, "'stop' requires one argument: machine name"))
            return ERR_INVALID_PARAM_COUNT;
        success = handler.stopMachine(argv[2]);
    }
    //destroy a VM
    else if (action == "destroy") {
        if (!CheckArgCount(argc, 3, "'destroy' requires one argument: machine name"))
            return ERR_INVALID_PARAM_COUNT;
        success = handler.destroyMachine(argv[2]);
    }
    //print help
    else if (action == "-h" || action == "--help" || action == "help") {
        PrintHelp();
        return ERR_OK;
    }
    //unknown action
    else {
        std::cerr << "Invalid operation\n";
        return ERR_INVALID_OPERATION;
    }

    if (success)
        return ERR_OK;
    else
        return ERR_RUNTIME_ERROR;
}


void PrintHelp() {
    std::cout << "Usage: cernvm-launch OPTION\n"
              << "OPTIONS:\n"
              << "\tcreate [--no-start] CONFIGURATION_FILE USER_DATA_FILE\tCreate a machine with specified configuration and user data.\n"
              << "\tdestroy MACHINE_NAME\tDestroy an existing machine.\n"
              << "\tlist [MACHINE_NAME]\tList all existing machines or a detailed info about one.\n"
              << "\tstart MACHINE_NAME\tStart an existing machine.\n"
              << "\tstop MACHINE_NAME\tStop an existing machine.\n"
              << "\t-h, --help\tPrint this help message.\n";
}


//Checks if the argument count is same as desired. If not, it prints
//the errorMessageOnFail on the stderr (with a newline).
//On success returns true.
bool CheckArgCount(int argc, int desiredCount, const std::string& errorMessageOnFail) {
    if (argc != desiredCount) {
        std::cerr << errorMessageOnFail << std::endl;
        return false;
    }
    return true;
}

} //anonymous namespace
