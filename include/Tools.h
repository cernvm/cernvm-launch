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

    typedef std::map<const std::string, const std::string>  configMapType;
    typedef std::map<const std::string, const std::string>* configMapTypePtr;

    //TODO add description
    void             AddMissingValuesToMap(configMapType& outMap, const configMapType& sourceMap);
    configMapTypePtr GetGlobalConfig();
    bool             LoadGlobalConfig(std::map<const std::string, const std::string>& outMap);
    bool             LoadFileIntoMap(const std::string& filename, std::map<const std::string, const std::string>& outMap);
    bool             LoadFileIntoString(const std::string& filename, std::string& output);
    void             PrintParameters(const std::vector<std::string>& fields, const ParameterMapPtr paramMap);
} //namespace Tools
} //namespace Launch

#endif //_TOOLS_H
