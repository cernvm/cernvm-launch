#include <iostream>
#include <fstream>
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

    if (action == "list")
        handler.listCvmMachines();
    else if (action == "create") {
        if (!CheckArgCount(argc, 4, "'create' requires two arguments: configuration_file and userData_file"))
            return ERR_INVALID_PARAM_COUNT;
        handler.createMachine(argv[2], argv[3]);
    }
    else if (action == "start") {
        if (!CheckArgCount(argc, 3, "'start' requires one argument: machine name"))
            return ERR_INVALID_PARAM_COUNT;
        handler.startMachine(argv[2]);
    }
    else if (action == "stop") {
        if (!CheckArgCount(argc, 3, "'stop' requires one argument: machine name"))
            return ERR_INVALID_PARAM_COUNT;
        handler.stopMachine(argv[2]);
    }
    else if (action == "delete") {
        if (!CheckArgCount(argc, 3, "'delete' requires one argument: machine name"))
            return ERR_INVALID_PARAM_COUNT;
        handler.deleteMachine(argv[2]);
    }
    else if (action == "-h" || action == "--help") {
        PrintHelp();
        return ERR_OK;
    }
    else {
        std::cerr << "Invalid operation\n";
        return ERR_INVALID_OPERATION;
    }

    return ERR_OK;
}


void PrintHelp() {
    std::cout << "cernvm-launch usage:\tTODO\n"; //TODO complete the help
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
