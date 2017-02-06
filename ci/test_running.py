#!/usr/bin/env python

import os, sys, subprocess, optparse

# Notes:
# - We rely on the default shared folder existence: it has to map to our home dir and its name in the VM is '<VM_NAME>_sf'
# - Local VBoxManage command has to be available in the PATH

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
# Script to run from the inside of the VM
INSIDE_SCRIPT = os.path.join(THIS_DIR, 'run_inside_vm.py')


# Return a VirtualBox VBoxManage binary path
def GetVBoxBinary():
    paths = (
        # Windows paths
        "C:/Program Files/Oracle/VirtualBox",
        "C:/Program Files (x86)/Oracle/VirtualBox",
        # MacOS paths
        "/Applications/VirtualBox.app/Contents/MacOS",
        "/Applications/Utilities/VirtualBox.app/Contents/MacOS",
        # Linux paths
        "/bin",
        "/usr/bin",
        "/usr/local/bin",
        "/opt/VirtualBox/bin",
    )
    # Possible names of the VBox executable
    binaries = (
        "VBoxManage.exe",
        "vboxmanage.exe",
        "VBoxManage",
        "vboxmanage",
    )

    for path in paths:
        for binary in binaries:
            binPath = os.path.join(path, binary)
            if os.path.exists(binPath) and os.path.isfile(binPath):
                # Possible candidate, try to run a simple cmd
                _, __, ec = RunCmd([binPath, "--version"])
                if ec == 0:
                    return binPath

    return None  # Not found


# Return a list of names of running VMs
def GetRunningVms(vboxBin):
    if not vboxBin:
        return []

    cmd = (
        vboxBin,
        "list",
        "runningvms",
    )
    stdout, stderr, ec = RunCmd(cmd)
    if ec != 0 or not stdout.strip():
        return []

    vmNames = []
    # Extract only VM names, output format: "<VM_NAME>" {<UID>} , e.g.: "test" {7127b4bb-cf95-4463-a736-77c02354f4b0}
    for line in stdout.strip().split('\n'):
        # Get the VM name (string between the quotes)
        vmName = line.split('"')[1].split('"')[0]
        if vmName:
            vmNames.append(vmName)

    return vmNames


# Return given parameters, terminate the script if the required parameters are not supplied
def GetScriptParams():
    vmName = None
    username = None
    password = None

    parser = optparse.OptionParser()
    parser.add_option("-m", "--machine", dest="vmName", help="VM name you want to test")
    parser.add_option("-u", "--user", dest="username", help="User used to log in into the VM")
    parser.add_option("-p", "--password", dest="password", help="Password used to log in into the VM")
    (options, args) = parser.parse_args()

    if options.vmName is None:
        print("Argument '-m' is mandatory")
        sys.exit(99)
    if options.username is None:
        print("Argument '-u' is mandatory")
        sys.exit(99)
    if options.password is None:
        print("Argument '-p' is mandatory")
        sys.exit(99)

    return options.vmName, options.username, options.password


# You can either run this script from the command line or by calling this function directly
def Main(vmName=None, username=None, password=None):
    # Try to get parameters from the script arguments (probably called from the command line)
    if not vmName or not username or not password:
        vmName, username, password = GetScriptParams()

    vbox = GetVBoxBinary()
    if not vbox:
        print("Unable to locate VBoxManage binary, make sure VirtualBox is installed in a default location")
        return 1

    runningVms = GetRunningVms(vbox)

    # Get a path relative to the user home directory
    userHome = os.path.expanduser('~')

    insideScriptRelPath = INSIDE_SCRIPT[ len(userHome) : ]  # trim a user home directory part
    if insideScriptRelPath[0] == '/' or insideScriptRelPath[0] == '\\':
        insideScriptRelPath = insideScriptRelPath[1:]

    # Create a script path from the VM perspective
    testScriptPath = os.path.join('/mnt/shared/%s_sf/' % vmName, insideScriptRelPath)

    print("Trying to run test script from inside VM: %s" % testScriptPath)

    if not vmName in runningVms:
        print("VM %s is not running, doing nothing" % vmName)
        return 2

    # Create a VBoxManage command, example:
    #VBoxManage guestcontrol test run --username atlas --password boson --exe /bin/bash -- bash /mnt/shared/test_sf/CERN/cernvm-launch/ci/do.sh

    cmd = (
        vbox,
        "guestcontrol",
        vmName,
        "run",
        "--username",
        username,
        "--password",
        password,
        "--exe",
        "/usr/bin/python",  # which binary to execute
        "--",
        "python",  # argv[0]
        testScriptPath,  # argv[1], path to our local (on the host OS) script, accessible via shared folder
    )

    stdout, stderr, ec = RunCmd(cmd)
    if ec == 0:
        print("\nVM running test successful, see the output:\n")
        print(stdout.strip().replace('\n', '\n\t'))
        return 0
    else:
        print("\nAn error occurred:", stderr)
        return 3


# Run given command (list of arguments) in the OS shell
def RunCmd(cmdList):
    # stdin and stderr are inherited from the parent
    pipe = subprocess.Popen(cmdList, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = pipe.communicate()
    pipe.wait()

    return stdout, stderr, pipe.returncode


if __name__ == "__main__":
    sys.exit(Main())
