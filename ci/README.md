CernVM-Launch: Continuous Integration
=====================================

Content of this directory is specific to the CernVM Continuous Integration setup.

We have two jobs in Jenkins, both of them are started only on demand:
- CernVM-Launch
- CernVM-Launch_tests

CernVM-Launch Job
-----------------
The Jenkins job does the following:
- Checkout the Git repository
- Start build on preselected machines (Win7, SLC5, Mac OS X).
- Runs Python2.6 script `build.py`, which has following steps:
    - Run a prepare script from the `./ci` directory (machine and OS specific), which creates the make environment.
    - Run a cmake build command.

If this job is successful (i.e. all the builds were successful), it automatically triggers the test job.


CernVM-Launch_tests Job
-----------------------
This job depends on a previous successful execution of the CernVM-Launch job, which environment (workspace) has to be preserved.

- Go to the CernVM-Launch job workspace
- Run Python2.6 script `test.py`, which has following steps:
    - Go through `./ci/tests/` directory
    - For each `.ini` file found, start the test (see the format below)
- If all the tests were successful, upload the cernvm-launch binaries to https://ecsft.cern.ch/dist/cernvm/launch/bin/

### Test if a VM is running

After all tests in the `./ci/tests/` directory are passed, we test whether we can run a command inside a created VM.
Internally, the `test.py` script creates a `launch_running_machine` VM via CernVM-Launch utility and then runs
an external command (`VBoxManage guestcontrol`), which simply prints out the `/etc/issue` file (from inside of the VM). This external
command runs the script `run_inside_vm.py` in the host OS, which is accessed via the default shared folder.

**Important: Currently we only run this test on the Mac machine, because it is our only real physical machine.**

### Test file format

Test file can have several sections, which are executed in the same order as they occur.
Section represents one run of the cernvm-launch binary. Test is successful if all its
sections run successfully. Test must not leave any artifacts after its execution (e.g.
created machines).

Section has following parameters:
- cmd_params: cernvm-launch command line arguments. You can provide a 
  `file:` macro, which is expanded to the `./ci/tests/` directory during runtime.
- expected_ec: cernvm-launch expected return code (if the command above runs successfully).
- expected_output_reqex: cernvm-launch expected output (if the command above runs successfully).
  This regular expression follows the Python re module specification: https://docs.python.org/2/library/re.html#regular-expression-syntax


Test file example, which creates, lists and destroys the machine:

    [create_machine]
    # Create a machine called 'launch_testing_machine'
    cmd_params = create --no-start file:userData.conf file:params.conf
    expected_ec = 0

    [list_machine]
    cmd_params = list launch_testing_machine
    expected_ec = 0
    expected_output_regex = ".*launch_testing_machine:\s+CVM:.*port:.*"

    [destroy_machine]
    cmd_params = destroy launch_testing_machine
    expected_ec = 0

