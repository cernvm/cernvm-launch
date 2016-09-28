/**
 * Main function, which handles parameters and invokes appropriate functionality.
 * Author: Petr Jirout, 2016
 */

#include <iostream>
#include <string>
#include <map>

#include <CernVM/Utilities.h>
#include <CernVM/Hypervisor.h>
#include <CernVM/ParameterMap.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxCommon.h>
#include <CernVM/Hypervisor/Virtualbox/VBoxSession.h>

#include "Tools.h"
#include "RequestHandler.h"

using namespace Launch;


namespace {

//CernVM-Launch version
const std::string VERSION = "0.1.0";

//Module local functions
bool             CheckArgCount(int argc, int desiredCount, const std::string& errorMessageOnFail);
int              DispatchArguments(int argc, char** argv, Launch::RequestHandler& handler);
void             PrintHelp();
void             PrintVersion();

//list of error codes returned by the program
const int ERR_OK = 0; //success
const int ERR_INVALID_PARAM_COUNT = 1;
const int ERR_INVALID_OPERATION = 2;
const int ERR_RUNTIME_ERROR = 3;


} //anonymous namespace


int main(int argc, char** argv) {
    Tools::configMapTypePtr configMap = Tools::GetGlobalConfig();
    if (configMap) {
        if (configMap->find("launchHomeFolder") != configMap->end()) {
            //Initialize the libcernvm path
            bool ret = setAppDataBasePath(configMap->at("launchHomeFolder"));
            if (! ret)
                std::cerr << "Unable to set launchHomeFolder to: " << configMap->at("launchHomeFolder") << std::endl;
        }
    }

    Launch::RequestHandler handler;

    int exitCode = DispatchArguments(argc, argv, handler);

    return exitCode;
}


namespace {

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
    if (action == "list") {
        if (argc == 3) {
            if (std::string(argv[2]) == "--running") //list only running machines
                success = handler.listRunningCvmMachines();
            else //the user requested details of a machine
                success = handler.listMachineDetail(argv[2]);
        }
        else
            success = handler.listCvmMachines();
    }
    //create a VM
    else if (action == "create") {
        //handler.createMachine(useData, boolStartOpt, paramFileOpt)
        //Generic format: ./cernvm-launch create [--no-start] userData_file [config_file]
        if (argc < 3) {
            std::cerr << "'create' requires at least a 'user_data_file' argument" << std::endl;
            return ERR_INVALID_PARAM_COUNT;
        }
        bool noStartFlag = false;
        if (std::string(argv[2]) == "--no-start")
            noStartFlag = true;

        //./cernvm-launch create user_data_file
        if (argc == 3) {
            if (noStartFlag) { // we have only --no-start flag
                std::cerr << "'create' requires at least a 'user_data_file' argument" << std::endl;
                return ERR_INVALID_PARAM_COUNT;
            }
            success = handler.createMachine(argv[2], true);
        }
        //./cernvm-launch create userData.conf params.conf or ./cernvm-launch create --no-start user_data_file
        else if (argc == 4) {
            if (noStartFlag) // --no-start and userData
                success = handler.createMachine(argv[3], false);
            else // userData paramData
                success = handler.createMachine(argv[2], true, argv[3]);
        }
        //./cernvm-launch create --no-start config_file userData_file
        else if (argc == 5) {
            if (noStartFlag)
                success = handler.createMachine(argv[3], false, argv[4]);
            else {
                std::cerr << "'create' requires at most 3 parameters" << std::endl;
                return ERR_INVALID_PARAM_COUNT;
            }
        }
        else
            return ERR_INVALID_PARAM_COUNT;
    }
    //pause a VM
    else if (action == "pause") {
        if (!CheckArgCount(argc, 3, "'pause' requires one argument: machine name"))
            return ERR_INVALID_PARAM_COUNT;
        success = handler.pauseMachine(argv[2]);
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
        if (argc == 4 && std::string(argv[2]) == "--force") // ./cernvm-launch destroy --force machine_name
            success = handler.destroyMachine(argv[3], true); // force true
        else if (!CheckArgCount(argc, 3, "'destroy' requires one argument: machine name"))
            return ERR_INVALID_PARAM_COUNT;
        else // ./cernvm-launch destroy machine_name
            success = handler.destroyMachine(argv[2], false);
    }
    //print help
    else if (action == "-h" || action == "--help" || action == "help") {
        PrintHelp();
        return ERR_OK;
    }
    //print version
    else if (action == "-v" || action == "--version") {
        PrintVersion();
        return ERR_OK;
    }
    //unknown action
    else {
        std::cerr << "Invalid operation\n\n";
        PrintHelp();
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
              << "\tcreate [--no-start] USER_DATA_FILE [CONFIGURATION_FILE]\tCreate a machine with specified user data.\n"
              << "\tdestroy [--force] MACHINE_NAME\tDestroy an existing machine.\n"
              << "\tlist [--running] [MACHINE_NAME]\tList all existing machines or a detailed info about one.\n"
              << "\tpause MACHINE_NAME\tPause a running machine.\n"
              << "\tstart MACHINE_NAME\tStart an existing machine.\n"
              << "\tstop MACHINE_NAME\tStop a running machine.\n"
              << "\t-v, --version\t\tPrint version.\n"
              << "\t-h, --help\t\tPrint this help message.\n";
}


void PrintVersion() {
    std::cout << "CernVM-Launch " << VERSION << std::endl;
}

} //anonymous namespace
