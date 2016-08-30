#!/usr/bin/env python

# Cernvm-launch repository url
REPO_URL="https://github.com/cernvm/cernvm-launch/"
# Branch to clone from the repository
BRANCH= "devel"
# Where to clone the repository
REPO_DIR="./cernvm-launch/"


import os, sys, subprocess


# Clones the BRANCH of cernvm-launch repository to the REPO_DIR
def CloneRepo():
    print("Cloning repository: %s" % REPO_URL)

    cmd = ["git", "clone", "-b", BRANCH, REPO_URL, REPO_DIR]
    out, err, ec = RunCmd(cmd)

    if ec != 0:
        PrintErr(err)
        return False

    return True


# Run platform and machine dependant preparation/build script.
def RunBuildScript():
    prepareScript = REPO_DIR  # common prefix

    if RunningOnWin():
        prepareScript += "ci/prepare-vs2013-vt120_xp.bat"
    elif RunningOnMac():
        prepareScript += "ci/prepare-osx.sh"
    elif RunningOnLinux():
        prepareScript += "ci/prepare-linux64.sh"
    else:
        PrintErr("Unknown OS: %s" % os.name)
        return -1

    print("Running build script: %s" % prepareScript)
    out, err, ec = RunCmd([prepareScript])

    print("out:")
    print(out)
    print("err:")
    print(err)
    print("ec:")
    print(ec)

    return ec


# Main function, returned value is also the exit code of the script
def Main():
    #if not CloneRepo():
    #    PrintErr("Unable to clone the repository")
    #    return 1

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
    pipe = subprocess.Popen(cmdList, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = pipe.communicate()

    return stdout, stderr, pipe.returncode


# Prints given errMsg to stderr, followed by a newline
def PrintErr(errMsg):
    sys.stderr.write(errMsg)
    sys.stderr.write('\n')


# Main function wrapper
if __name__ == "__main__":
    sys.exit(Main())

