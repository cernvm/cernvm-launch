cmake_minimum_required (VERSION 2.8)

# Guess system architecture
set(GUESS_ARCH "x86_64")						# 1) Fallback to x86_64
if(CMAKE_SYSTEM_PROCESSOR)                      # 2) Build on the same processor
	set(GUESS_ARCH ${CMAKE_SYSTEM_PROCESSOR})
	if(${GUESS_ARCH} STREQUAL "AMD64")
		# We don't do any differentiation for AMD64 instruction set
		set(GUESS_ARCH "x86_64")
	endif()
endif()
if(CMAKE_OSX_ARCHITECTURES)						# 3) Lookup on OSX Architectures
	set(GUESS_ARCH ${CMAKE_OSX_ARCHITECTURES})
elseif ("${CMAKE_GENERATOR}" MATCHES "Win64") 	# 4) Lookup on Windows Generator
	set(GUESS_ARCH "x86_64")
endif()

# Prompt architecture
set(TARGET_ARCH "${GUESS_ARCH}" CACHE STRING "Override the identified target architecture (x86_64 or i386)" )
message(STATUS "Building cernvm-launch for arch: ${TARGET_ARCH}")

# Change OSX architectures
if (APPLE)
	set(CMAKE_OSX_ARCHITECTURES ${TARGET_ARCH})
	set(OSX_ARCHITECTURES ${TARGET_ARCH})
endif()

# Logging option
option(LOGGING "Set to ON to enable verbose logging on screen" OFF)
option(CRASH_REPORTING "Set to ON to enable crash reporting" OFF)
set(SYSCONF_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/etc" CACHE STRING "The /etc configuration directory")

# CernVM-Launch
project (cernvm-launch)
set(VERSION "0.0.1")

############################################################################################################ 
# AddCompileLinkFlags module
############################################################################################################
# Append str to a string property of a target. 
# target:      string: target name. 
# property:            name of target’s property. e.g: COMPILE_FLAGS, or LINK_FLAGS 
# str:         string: string to be appended to the property 
macro(append_target_property target property str) 
  get_target_property(current_property ${target} ${property}) 
  if(NOT current_property) # property non-existent or empty 
      set_target_properties(${target} PROPERTIES ${property} ${str}) 
  else() 
      set_target_properties(${target} PROPERTIES ${property} "${current_property} ${str}") 
  endif() 
endmacro(append_target_property)

############################################################################################################ 
# Add/append compile flags to a target. 
# target: string: target name. 
# flags : string: compile flags to be appended 
macro(add_compile_flags target flags) 
  append_target_property(${target} COMPILE_FLAGS ${flags}) 
endmacro(add_compile_flags)

############################################################################################################ 
# Add/append link flags to a target. 
# target: string: target name. 
# flags : string: link flags to be appended 
macro(add_link_flags target flags) 
  append_target_property(${target} LINK_FLAGS ${flags}) 
endmacro(add_link_flags) 



#############################################################
# CERNVM LIBRARY 
#############################################################

# CernVM Library sources
set( CERNVM_LIBSRC "extern/libcernvm" CACHE STRING "Specify the root directory of the libcernvm sources" )
set( USE_SYSTEM_LIBS 0 )

# Include sub-project
add_subdirectory( ${CERNVM_LIBSRC} libcernvm )
include_directories( ${CERNVM_INCLUDE_DIRS} )

#############################################################
# LIBRARIES 
#############################################################

# For every library, we give the option to the project that
# includes us to provide them by itself.
#
# If not, we will try our best to build them as our child
# libraries (statically).
#
# To define a library, you must define the following two directives:
#   XXXXXX_LIBRARIES : Which contain the name(s) of the libraries
#	XXXXXX_INCLUDE 	 : Which contain the additiona include directories
#

# So far, we don't need other libraries. If so, please check cernvm-webapi
# or libcernvm CMakeLists.txt for reference.


#############################################################
# SOURCES 
#############################################################

# Add custom definitions
#if (LOGGING)
#	add_definitions(-DLOGGING)
#endif()
#if (CRASH_REPORTING)
#	add_definitions(-DCRASH_REPORTING)
#endif()

# Fixes for windows
#if (WIN32)
	# Fix: Disable Min/Max macros
    #	ADD_DEFINITIONS(-DNOMINMAX)   
	# Fix: For some reason CURL_STATICLIB is not defined
    #	ADD_DEFINITIONS(-DCURL_STATICLIB)
#endif(WIN32)

# Locate platform-dependant sources
if (WIN32)
	set ( PLATFORM_DIR ${PROJECT_SOURCE_DIR}/src/platform/win )
	file ( GLOB PLATFORM_SOURCES ${PLATFORM_DIR}/*.cpp ${PLATFORM_DIR}/*.rc )
elseif (APPLE)
	set ( PLATFORM_DIR ${PROJECT_SOURCE_DIR}/src/platform/osx )
	file ( GLOB PLATFORM_SOURCES ${PLATFORM_DIR}/*.mm  )
elseif (UNIX)
	set ( PLATFORM_DIR ${PROJECT_SOURCE_DIR}/src/platform/unix )
	file ( GLOB PLATFORM_SOURCES ${PLATFORM_DIR}/*.cpp  )
endif()


# Locate the daemon sources
file (GLOB LAUNCH_SOURCE_FILES 
		${PROJECT_SOURCE_DIR}/src/*.cpp
	)

file (GLOB LAUNCH_HEADER_FILES
		${PROJECT_SOURCE_DIR}/inc/*
	)

set(LAUNCH_SOURCE ${LAUNCH_SOURCE_FILES} ${LAUNCH_HEADER_FILES})

# Setup includes
include_directories( ${PROJECT_SOURCE_DIR}/inc )
include_directories( ${PROJECT_INCLUDES} )

message(STATUS "PROJECT_NAME:${PROJECT_NAME}")

# Create static library
SET(BUILD_SHARED_LIBRARIES OFF)

#############################################################
# TARGET & LINK
#############################################################

# Add executable, depending on the platform
if (APPLE)
	# Mac OSX Foundations
	find_library(FRAMEWORK_FOUNDATION NAMES Foundation)
	find_library(FRAMEWORK_COCOA NAMES Cocoa)

	# Sources
	add_executable( ${PROJECT_NAME} MACOSX_BUNDLE
		${PLATFORM_SOURCES}
		${LAUNCH_SOURCE}
		)

elseif (WIN32)
    #Fix: On CMake 3.5 it's trying to link dynamically
    if (MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif()

    # Sources
	add_executable( ${PROJECT_NAME} WIN32
		${PLATFORM_SOURCES}
		${LAUNCH_SOURCE}
		)

else() #Linux
	# Sources
	add_executable( ${PROJECT_NAME}
		${PLATFORM_SOURCES}
		${LAUNCH_SOURCE}
		)

endif()

# On linux we should add a flag to define the architecture we are building for
if (UNIX)
	if ("${TARGET_ARCH}" STREQUAL "x86_64")
		add_compile_flags(${PROJECT_NAME} -m64)
	else()
		add_compile_flags(${PROJECT_NAME} -m32)
	endif()
endif()

# Post-target Fixes for windows
if (WIN32)
    # Fix: OpenSSL builds are not built with safe exception handlers
    # Fix2: without subsystem:console in does not find main function
    add_link_flags( ${PROJECT_NAME} "/SAFESEH:NO /SUBSYSTEM:CONSOLE" )
endif()

# Enable C++98 standard
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++98" COMPILER_SUPPORTS_CXX98)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
CHECK_CXX_COMPILER_FLAG("-Wno-deprecated" COMPILER_SUPPORTS_NO_DEPRECATED)
if(COMPILER_SUPPORTS_CXX11)
	add_compile_flags( ${PROJECT_NAME} -std=c++11 )
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has C++11 standard flag support.")
elseif(COMPILER_SUPPORTS_CXX0X)
	add_compile_flags( ${PROJECT_NAME} -std=c++0x )
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has C++0x standard flag support.")
elseif(COMPILER_SUPPORTS_CXX98)
	add_compile_flags( ${PROJECT_NAME} -std=c++98 )
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has C++98 standard flag support.")
endif()

if(COMPILER_SUPPORTS_NO_DEPRECATED) #Because of Boost's usage of auto_ptr, which is deprecated
    add_compile_flags( ${PROJECT_NAME} -Wno-deprecated )
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has -Wno-deprecated flag support, using it.")
endif()


# Libraries
target_link_libraries ( ${PROJECT_NAME} ${CERNVM_LIBRARIES} )
target_link_libraries ( ${PROJECT_NAME} ${PROJECT_LIBRARIES} )

# Link OSX Frameworks
if (APPLE)
	target_link_libraries ( ${PROJECT_NAME} ${FRAMEWORK_FOUNDATION} )
	target_link_libraries ( ${PROJECT_NAME} ${FRAMEWORK_COCOA} )
	set_target_properties( ${PROJECT_NAME} PROPERTIES 
		MACOSX_BUNDLE_INFO_PLIST ${PLATFORM_DIR}/Info.plist
	)

	# Embed icon in the archive
    #	set_source_files_properties(
    #		${PLATFORM_DIR}/icon.icns PROPERTIES
    #		MACOSX_PACKAGE_LOCATION Resources
    #)
endif()

# Installation rules for linux (for archive packaging)
if (UNIX AND NOT APPLE)
	# Binary archive
	install(
		TARGETS
        	cernvm-launch
        RUNTIME DESTINATION
			${CMAKE_INSTALL_PREFIX}/bin
		)

	# Resources (icons)
#	install(
#		FILES
#			${PLATFORM_DIR}/icon.png
#			${PLATFORM_DIR}/icon.svg
#		DESTINATION
#			${CMAKE_INSTALL_PREFIX}/share/cernvm-webapi
#		)

endif()
