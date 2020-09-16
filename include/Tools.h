/**
 * This module contains various helper functions.
 * Author: Petr Jirout, 2016
 */

#ifndef _TOOLS_H
#define _TOOLS_H

#include <string>
#include <map>
#include <vector>

#include <CernVM/ParameterMap.h>


//Configuration file defines
#define KEY_VALUE_SEPARATOR '='
#define COMMENT_CHAR        '#'

namespace Launch {
namespace Tools {
    // All methods return true on success, false otherwise.

    typedef std::map<const std::string, const std::string>  configMapType;
    typedef std::map<const std::string, const std::string>* configMapTypePtr;

    //Go through sourceMap and add values, which are not already present in the outMap
    void             AddMissingValuesToMap(configMapType& outMap, const configMapType& sourceMap);
    //Create a default global config file
    bool             CreateDefaultGlobalConfig();
    //Returns a singleton instance of global config map. Of the first call it tries to load it
    configMapTypePtr GetGlobalConfig();
    //Prompts user for a value (terminated by Enter) and stores it outValue
    bool             GetUserInput(std::string& outValue);
    //Check if given path is absolute
    bool             IsAbsolutePath(const std::string& path);
    //Check if given path is canonical
    bool             IsCanonicalPath(const std::string& path);
    //Make absolute path from a given relative one
    bool             MakeAbsolutePath(const std::string& path, std::string& outPath);
    //Load the global config file.
    bool             LoadGlobalConfig(std::map<const std::string, const std::string>& outMap);
    //Load given file into the outMap
    //Expected format is key=value
    //Comments (line starting with '#') and empty values are ignored
    bool             LoadFileIntoMap(const std::string& filename, std::map<const std::string, const std::string>& outMap);
    //Load given file into a string
    bool             LoadFileIntoString(const std::string& filename, std::string& output);
    //Print specified fields from the given paramMap
    void             PrintParameters(const std::vector<std::string>& fields, const ParameterMapPtr paramMap);
    //Set additional binary mask flags in the given string
    bool             SetFlagsInString(std::string& flagsStr, int additionalFlags);

    std::vector<std::string> SplitString(const std::string &str, const char delim,
                                         const unsigned max_chunks);
} //namespace Tools
} //namespace Launch

#endif //_TOOLS_H
