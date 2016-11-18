/**
 * This module contains various helper functions.
 * Author: Petr Jirout, 2016
 */

#include <fstream>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <CernVM/Utilities.h>

#include "Tools.h"


namespace Launch {
namespace Tools {

//Global config map singleton object
configMapType GlobalConfigMap;

//systemPath changes the slashes to correct ones
const std::string GLOBAL_CONFIG_FILENAME = systemPath(getHomeDir() + "/.cernvm-launch.conf");

//You need to stitch these two part together, with home folder in the middle (we prompt the user for it)
const std::string defaultConfigFileStrPartOne = \
"########### CernVM-Launch configuration ###########\n"
"# Folder on the host OS which will be shared to VMs\n"
"sharedFolder=" + getHomeDir() + "\n"
"# Folder on the host OS where all VM configuration files and images are stored (can get large)\n"
"# Changing this folder will disconnect already existing machines from CernVM-Launch\n";
const std::string defaultConfigFileStrPartTwo = \
"########### Default VM parameters ###########\n"
"# VM's port connected to the host OS. Use 22 to have SSH access to the machine\n"
"apiPort=22\n"
"cernvmVersion=latest\n"
"cpus=1\n"
"memory=2048\n"
"disk=20000\n"
"executionCap=100\n"
"# Flags: 64bit, headful mode, graphical extensions\n"
"flags=49\n";


//Add non-existent items from sourceMap to outMap
void AddMissingValuesToMap(configMapType& outMap, const configMapType& sourceMap) {
    configMapType::const_iterator it = sourceMap.begin();
    for (; it != sourceMap.end(); ++it) {
        if (outMap.find(it->second) == outMap.end())
            outMap.insert(std::make_pair(it->first, it->second));
    }
}


bool CreateDefaultGlobalConfig() {
    std::ofstream ofs (GLOBAL_CONFIG_FILENAME);
    if (!ofs.good()) //error when opening the file
        return false;

    std::cout << "Creating a new global config: " << GLOBAL_CONFIG_FILENAME << std::endl;
    std::string launchDir;

    std::cout << "Enter a directory where do you want keep all CernVM-Launch files: VM images, disk files, etc. "
              << "These files can grow substantially.\n"
              << "Enter directory [" << getDefaultAppDataBaseDir() << "]: ";
    if (!GetUserInput(launchDir))
        launchDir = getDefaultAppDataBaseDir();
    else if (! IsCanonicalPath(launchDir) && ! IsAbsolutePath(launchDir)) {
        std::string defaultPath = getDefaultAppDataBaseDir();
        std::cerr << "Given path '" << launchDir << "' is not an absolute path, using default: '" << defaultPath << "'.\n";
        std::cerr << "You can change it later in the config file.\n";
        launchDir = defaultPath;
    }

    if (!launchDir.empty())
        launchDir = "launchHomeFolder=" + launchDir + "\n";

    ofs << defaultConfigFileStrPartOne << launchDir << defaultConfigFileStrPartTwo; //ofs is closed on object destroy

    return true;
}


//Get a pointer to the global config object, load if necessary
configMapTypePtr GetGlobalConfig() {
    if (GlobalConfigMap.empty()) {
        bool configLoaded = LoadGlobalConfig(GlobalConfigMap);
        if (!configLoaded) { //unable to load the config, create a default one
            if (!CreateDefaultGlobalConfig()) { //unable to write a global config file
                std::cerr << "Unable to create default config\n";
                return NULL;
            }
            GlobalConfigMap.clear();
        }
        configLoaded = LoadGlobalConfig(GlobalConfigMap);
        if (!configLoaded) { //unable to load the newly created config
            std::cerr << "Unable to load newly created config\n";
            return NULL;
    }
    }
    return &GlobalConfigMap;
}


//Get input from user (stdin) and trim it
bool GetUserInput(std::string& outValue) {
    std::getline(std::cin, outValue);
    boost::trim(outValue);
    if (outValue.empty())
        return false;

    return true;
}

//Check if the path is absolute
bool IsAbsolutePath(const std::string& path) {
    boost::filesystem::path absolutePath;
    try {
        absolutePath = boost::filesystem::absolute(path);
    }
    catch (boost::filesystem::filesystem_error& e) {
        return false;
    }

    return absolutePath == boost::filesystem::path(path);
}

//We construct an canonical path for the given path. If they differ, the given one was not canonical
//The path does have to exist
bool IsCanonicalPath(const std::string& path) {
    boost::filesystem::path canonicalPath;
    try {
        canonicalPath = boost::filesystem::canonical(path);
    }
    catch (boost::filesystem::filesystem_error& e) {
        return false;
    }

    return canonicalPath == boost::filesystem::path(path);
}

//Make an absolute path from a relative one
bool MakeAbsolutePath(const std::string& path, std::string& outPath) {
    boost::filesystem::path absolutePath;
    try {
        absolutePath = boost::filesystem::absolute(path);
    }
    catch (boost::filesystem::filesystem_error& e) {
        std::string errStr = e.what();
        std::cerr << errStr.substr(errStr.find(": ")+2) << std::endl; // strip 'boost::filesystem::absolute: '
        return false;
    }
    outPath = absolutePath.string();
    return true;
}


//Load global config file (with default VM parameters and Launch configuration)
bool LoadGlobalConfig(std::map<const std::string, const std::string>& outMap) {
    bool success = Tools::LoadFileIntoMap(GLOBAL_CONFIG_FILENAME, outMap);

    if (! success) {
        std::cout << "Unable to load global config file: " << GLOBAL_CONFIG_FILENAME << std::endl;
        return false;
    }

    return true;
}


//Load <string,string> map from a file with key=value items.
//Lines starting with '#' are considered as comments, thus ignored.
//Lines without KEY_VALUE_SEPARATOR (i.e. '=') are ignored as well
//Values can be quoted either with single or double quotes (quotes are discarded).
//Existing values in the map are overwrited with a new value
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
        //if the value is quoted, strip the quotes
        size_t valueSize = value.size();
        if (valueSize > 1 && (
                (value[0] == '"' && value[valueSize-1] == '"') ||    // double quotes
                (value[0] == '\'' && value[valueSize-1] == '\'') )) { // single quotes
            value.erase(0, 1); //front quote
            value.erase(value.size()-1); //end quote
        }

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
void PrintParameters(const std::vector<std::string>& fields, const ParameterMapPtr paramMap) {
    std::vector<std::string>::const_iterator it = fields.begin();
    for (; it != fields.end(); ++it) {
        std::string value = paramMap->get(*it, "");
        if (! value.empty())
            std::cout << "\t" << *it << ": " << value << std::endl;
    }
}

//Set additional binary mask flags in the given string
bool SetFlagsInString(std::string& flagsStr, int additionalFlags) {
    int numFlags;
    try {
        numFlags = std::stoi(flagsStr);
    }
    catch (...) {
        numFlags = 49;
    }
    numFlags |= additionalFlags;
    flagsStr = std::to_string(numFlags);

    return true;
}


} //namespace Tools
} //namespace Launch
