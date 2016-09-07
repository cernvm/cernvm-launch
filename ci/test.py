#!/usr/bin/env python

import os, sys, subprocess, re
import ConfigParser


# Directory where all the test files are (./ci/tests)
TEST_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tests")


# Main function, its exit code is also the exit code of the script
def Main():
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
        print("All tests successful")
    else:
        print("FAILED tests [%d/%d], i.e. %d%%:" % (mainEc, testFilesCount, 100*mainEc/testFilesCount))
        for file in failedTests:
            print("\t%s" % file[:-4]) # strip the '.ini'

    return mainEc


# Run all sections in the given test files and return whether it was successful or not
def RunTest(launchBinary, testFile):
    config = ConfigParser.ConfigParser()
    config.read(os.path.join(TEST_DIR, testFile))

    sections = config.sections()
    for section in sections:
        suc = ExecuteSection(launchBinary, config, section)
        if not suc: # stop executing when one section fails
            return False

    print("\tOK")
    return True


# Executes given section from the given config
# Returns true if the section run successfully.
def ExecuteSection(launchBinary, configParser, section):
    cmdParams = configParser.get(section, "cmd_params")
    cmdParams = RemoveQuotes(cmdParams)
    expEc = int(configParser.get(section, "expected_ec"))

    runCmdList = [launchBinary] + cmdParams.split() # split the cmdParams string and concatenate in
    expRegex = None # expected_output is optional
    if configParser.has_option(section, "expected_output_regex"):
        expRegex = configParser.get(section, "expected_output_regex")
        expRegex = RemoveQuotes(expRegex).strip()

    success = True
    stdout, stderr, ec = RunCmd(runCmdList)

    if ec != expEc:
        print("FAIL\tSection: %s" % section)
        print("\t\tError: Return code (%d) does not match the expected (%d) one" % (ec, expEc))
        success = False
    elif expRegex is not None: # check if the given regex matches
        pattern = re.compile(expRegex)
        if pattern.match(stdout) is None: #=> does not match
            print("FAIL\tSection: %s" % section)
            print("\t\tError: Output does not match the expected regex")
            print("\t\tReceived: %s" % stdout.strip())
            print("\t\tExpected: %s" % expRegex)
            success = False

    if not success:
        print("\t\tCommand args: %s" % cmdParams)

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


# Main function wrapper
if __name__ == "__main__":
    sys.exit(Main())
