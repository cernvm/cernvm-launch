#!/usr/bin/env python2.6

import os, sys, subprocess, re, time, shutil
import ConfigParser
try:
    # try with the standard library (Python2.7 and newer)
    from collections import OrderedDict
except ImportError:
    # fallback to Python 2.4-2.6 back-port
    from ordereddict import OrderedDict


# ./ci directory (where this script is) + added trailing path separator
CI_DIR = os.path.dirname(os.path.abspath(__file__)) + os.sep
# Directory where all the test files are (./ci/tests)
TEST_DIR = os.path.join(CI_DIR, "tests") + os.sep


# Main function, its exit code is also the exit code of the script
# This script accepts one optional parameter:
#       directory, where the binary is copied (under bin/${OS_TYPE}) if the tests are successful
def Main():
    binDestinationDir = None
    if len(sys.argv) > 1:
        binDestinationDir = sys.argv[1]
    # Try to locate a CernVM-Launch binary
    launchBinary = FindExecutable()
    if not launchBinary:
        print("Unable to find a CernVM-Launch binary")
        return -1
    else:
        print("CernVM-Launch executable: %s" % launchBinary)

    mainEc = 0
    failedTests = []
    # Go through '.ini' files in the test dir
    allFiles = os.listdir(TEST_DIR)
    testFilesList = []
    for file in allFiles: # extract only test config files
        if file.endswith(".ini"):
            testFilesList.append(file)

    testFilesCount = len(testFilesList)
    fileCount = 1
    for testFile in testFilesList:
        print(100*"=")
        print("Test [%d/%d]: %s" % (fileCount, testFilesCount, testFile[: -4])) # strip the '.ini'
        fileCount += 1

        success = RunTest(launchBinary, testFile)
        if not success:
            mainEc += 1
            failedTests.append(testFile)

    print(100*"=")
    if mainEc == 0:
        print("All static tests successful")
    else:
        print("FAILED tests [%d/%d], i.e. %d%%:" % (mainEc, testFilesCount, 100*mainEc/testFilesCount))
        for file in failedTests:
            print("\t%s" % file[:-4]) # strip the '.ini'

        return mainEc

    print(100*"=")
    mainEc = RunRunningTest()
    print(100*"=")

    if mainEc == 0:
        print("All tests successful")
        print(100*"=")
        CopyFileToBinDir(launchBinary, binDestinationDir)

    return mainEc


# Run running test: execute a script inside a running VM
# IMPORTANT: Currently we only run this test on Mac, because it's the only real physical machine
def RunRunningTest():
    from test_running import Main as RunningMain
    from test_running import GetVBoxBinary

    print("Test: Execute a script inside a VM")
    if not RunningOnMac():
        print("\tSkipped, not on a real VM")
        return 0

    # Create the machine
    launchBinary = FindExecutable()
    if not launchBinary:
        print("\tUnable to find a CernVM-Launch binary")
        return -1

    cmdList = (
        launchBinary,
        "create",
        "--name",
        "launch_running_machine",
        os.path.join(TEST_DIR, 'userData.conf'),
        os.path.join(TEST_DIR, 'params.conf'),
    )

    _, __, ec = RunCmd(cmdList)
    if ec != 0:
        print("\tUnable to create a testing VM")
        return 1
    # Sleep for 5 mins (wait for the VM to boot and contextualize)
    print("\tSleeping for 5 minutes, waiting for the testing VM to boot")
    time.sleep(300)

    # User atlas with pwd 'boson' is created by our user data file
    ec = RunningMain(vmName='launch_running_machine', username='atlas', password='boson')

    # Destroy the machine
    cmdList = (
        launchBinary,
        "destroy",
        "--force",
        "launch_running_machine",
    )

    _, __, ecc = RunCmd(cmdList)
    if ecc != 0:
        #Unable to destroy the testing VM via Launch utility, try VirtualBox
        vbox = GetVBoxBinary()
        if not vbox:
            print("\tUnable to find VirtualBox, you have to delete the 'launch_running_machine' VM by yourself")
            return 98
        cmdList = (
            vbox,
            "unregistervm",
            "launch_running_machine",
            "--delete",
        )
        _, __, ecc = RunCmd(cmdList)
        if ecc != 0:
            print("\tUnable to delete the VM, you have to delete the 'launch_running_machine' VM by yourself")

    return ec


#Copy given file to the bin/${OS_TYPE} directory
#If no binDestinationDir is specified, the ./ci dir is used
def CopyFileToBinDir(filePath, binDestinationDir=None):
    osType = "Unknown"
    if RunningOnWin():
        osType = "Win"
    elif RunningOnLinux():
        osType = "Linux"
    elif RunningOnMac():
        osType = "Mac"

    baseDir = CI_DIR
    if binDestinationDir:
        baseDir = binDestinationDir
    binDir = os.path.join(baseDir, 'bin', osType)

    if not os.path.exists(binDir):
        os.makedirs(binDir)
    try:
        shutil.copy(filePath, binDir)
    except shutil.Error: # ignore complain if the files are the same
        pass
    print("File %s copied to the bin directory: %s" % (filePath, binDir))


# Run all sections in the given test files and return whether it was successful or not
# Waits 5 seconds before executing next section (time for VirtualBox to manage things)
def RunTest(launchBinary, testFile):
    config = ConfigParser.ConfigParser(dict_type=OrderedDict) # specify dict_type to keep the order of sections
    config.read(os.path.join(TEST_DIR, testFile))

    good = True
    sections = config.sections()
    for section in sections:
        suc = ExecuteSection(launchBinary, config, section)
        if not suc:
            good = False
        if section != sections[-1]: # don't sleep after the last section
            time.sleep(5)

    if good:
        print("\tOK")
    return good


# Executes given section from the given config
# Returns true if the section run successfully
def ExecuteSection(launchBinary, configParser, section):
    cmdParams = configParser.get(section, "cmd_params")
    cmdParams = RemoveQuotes(cmdParams)
    cmdParams = cmdParams.split() # split the cmdParams string
    cmdParams = MacroReplace(cmdParams) # replace predefined macros
    expEc = int(configParser.get(section, "expected_ec"))

    runCmdList = [launchBinary] + cmdParams
    expRegex = None # expected_output is optional
    if configParser.has_option(section, "expected_output_regex"):
        expRegex = configParser.get(section, "expected_output_regex")
        expRegex = RemoveQuotes(expRegex).strip()

    success = True
    stdout, stderr, ec = RunCmd(runCmdList)

    if ec != expEc:
        print("FAIL\tSection: %s" % section)
        print("\t\tError: Return code (%d) does not match the expected (%d) one" % (ec, expEc))
        print("\t\tStderr: %s" % stderr)
        success = False
    elif expRegex is not None: # check if the given regex matches
        pattern = re.compile(expRegex, re.DOTALL)
        if pattern.match(stdout) is None: #=> does not match
            print("FAIL\tSection: %s" % section)
            print("\t\tError: Output does not match the expected regex")
            print("\t\tReceived: %s" % stdout.strip())
            print("\t\tExpected: %s" % expRegex)
            success = False

    if not success:
        print("\t\tCommand args: %s" % ' '.join(cmdParams))

    return success


# Tries to find platform dependent cernvm-launch executable
# It assumes it's being run from the ./ci subdirectory
def FindExecutable():
    rootDir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..") # one dir up
    if RunningOnWin():
        binName = "cernvm-launch.exe"
    else: # mac or linux
        binName = "cernvm-launch"

    for root, dirs, files in os.walk(rootDir):
        if binName in files:
            fullPath = os.path.join(root, binName)
            if os.path.isfile(fullPath):
                return fullPath

    return None


# Strip matching quotes from the beginning and end of the string (if they exist)
def RemoveQuotes(string):
    length = len(string)
    if length < 2: # too short string
        return string
    elif string[0] == '"' and string[length-1] == '"': # double quotes
        return string[1:length-1]
    elif string[0] == "'" and string[length-1] == "'": # single quotes
        return string[1:length-1]
    else: # no quotes
        return string


# Perform all macro replace operations on the given command list
def MacroReplace(cmdList):
    result = []
    for elem in cmdList:
        result.append(PathExpansion(elem))
    return result


# If the 'file:' pattern is present in the string, it gets replaced by the TEST_DIR
def PathExpansion(string):
    if "file:" in string:
        return string.replace("file:", TEST_DIR)
    return string


# Run given command (list of arguments) in the OS shell
def RunCmd(cmdList):
    # stdin and stderr are inherited from the parent
    pipe = subprocess.Popen(cmdList, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = pipe.communicate()
    pipe.wait()

    return stdout, stderr, pipe.returncode


# True if running on a Windows machine
def RunningOnWin():
    return sys.platform.startswith("win")


# True if running on a Mac OS machine
def RunningOnMac():
    return sys.platform.startswith("darwin") or os.name == "mac"


# True if running on a Linux machine
def RunningOnLinux():
    return sys.platform.startswith("linux")


# Main function wrapper
if __name__ == "__main__":
    sys.exit(Main())

