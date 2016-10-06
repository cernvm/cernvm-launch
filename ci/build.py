#!/usr/bin/env python2.6

import os, sys, subprocess


# Folder where the build files are created: parent directory to this file, build subdir
BUILD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build")


# Run platform and machine dependant preparation/build script.
def RunPreparationScript():
    ciDir = os.path.dirname(os.path.abspath(__file__))  # our config scripts are in the same dir as this file
    prepareScript = ""

    if RunningOnWin():
        prepareScript = "prepare-vs2013-vt120_xp.bat"
    elif RunningOnMac():
        prepareScript = "prepare-osx.sh"
    elif RunningOnLinux():
        prepareScript = "prepare-linux64.sh"
    else:
        PrintErr("Unknown OS: %s" % os.name)
        return -1

    prepareScript = os.path.join(ciDir, prepareScript)

    print("Preparation script: %s" % prepareScript)
    ec = RunCmd([prepareScript])

    return ec


# Run the build command in the build folder.
def RunBuildScript():
    buildCmd = ["cmake", "--build", BUILD_DIR]

    print("Build command: %s" % ' '.join(buildCmd))
    ec = RunCmd(buildCmd)

    return ec


# Main function, returned value is also the exit code of the script
def Main():
    ec = RunPreparationScript()
    if ec != 0:
        PrintErr("Error while running the preparation script")
        return ec

    ec = RunBuildScript()

    if ec != 0:
        PrintErr("Error while running the build script")

    return ec


# True if running on a Windows machine
def RunningOnWin():
    return sys.platform.startswith("win")


# True if running on a Mac OS machine
def RunningOnMac():
    return sys.platform.startswith("darwin") or os.name == "mac"


# True if running on a Linux machine
def RunningOnLinux():
    return sys.platform.startswith("linux")


# Run given command (list of arguments) in the OS shell
def RunCmd(cmdList):
    # stdin and stderr are inherited from the parent
    pipe = subprocess.Popen(cmdList)
    pipe.wait()

    return pipe.returncode


# Prints given errMsg to stderr, followed by a newline
def PrintErr(errMsg):
    sys.stderr.write(errMsg)
    sys.stderr.write('\n')


# Main function wrapper
if __name__ == "__main__":
    sys.exit(Main())

