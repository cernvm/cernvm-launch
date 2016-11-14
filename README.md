CernVM-Launch
=============

CernVM command line utility to control VirtualBox CernVMs.

Requirements
============

- VirtualBox has to be installed in a default location.

Usage
=====

CernVM-Launch provides following operations.


Create a virtual machine
------------------------

	create [--no-start] [--name MACHINE_NAME] [--memory NUM_MB] [--disk NUM_MB]
           [--cpus NUM] [--sharedFolder PATH] USER_DATA_FILE [CONFIGURATION_FILE]
		
Create a machine with specified user (contextualization) data.
By default, the machine is started right away (use `--no-start` to suppress that).

When creating a machine, you may specify the parameters in several ways via:
- command line arguments,
- configuration file,
- global CernVM-Launch config file.

Parameters are considered in the following precedence:

Command line parameters > configuration file > global config > hardcoded defaults.

When specifying `sharedFolder`, you must use a **canonical path**.

### Configuration file
When creating a machine, you can specify the creation parameters in a configuration file.

Creation parameters file example with all recognized items:

    name=launch_testing_machine
    apiPort=22
    cernvmVersion=2.6-9
    cpus=1
    memory=512
    disk=10000
    diskChecksum=
    sharedFolder=
    diskURL=
    diskChecksum=
    diskPath=
    executionCap=100
    #Flags: 64bit, guest additions, (headless mode)
    flags=5


### Hardcoded default parameters
If a user does not provide all of the parameters (neither through one of the three options), hardcoded defaults are used in that case:

    apiPort=22
    cernvmVersion=latest
    cpus=1
    memory=2048
    disk=20000
    executionCap=100
    # Flags: 64bit host OS, headful mode, graphical extensions enabled
    flags=49


### Flags parameter
There is one special parameter called `flags`, which uses a bit mask format for configuring several VM options.

    Active bit   Effect
         1       The system is 64-bit instead of 32-bit
         2       Use regular deployment (HDD) from an online source (instead of micro-iso)
         4       Include a guest additions CD-ROM
         8       Use floppyIO instead of contextualization CD-ROM
        16       Start the VM in headful mode
        32       Enable graphical extension (like drag-n-drop)
        64       Use secondary adapter instead of creating a NAT rule on the first one
       128       Use ttyS0 as external logfile.
       256       Use a bootable VDI file as the main deployment image.

If you want to use online source deployment, you need to specify the `diskURL` and `diskChecksum` parameters.

If you want to use a bootable VDI file, you need to provide the `diskPath` parameter.


Create a virtual machine through OVA image import
-------------------------------------------------

	import [--no-start] [--name MACHINE_NAME] [--memory NUM_MB] [--disk NUM_MB]
           [--cpus NUM] [--sharedFolder PATH] OVA_IMAGE_FILE [CONFIGURATION_FILE]

When a machine is created via OVA import, no contextualization is done. The OVA image is also
expected to be bootable.

Configuration file has the same format as in the `create` operation.

Destroy an existing VM
-----------------------

	destroy [--force] MACHINE_NAME
	
Destroy an existing VM. If the machine is running, the user is prompted with confirmation.
If want to avoid the prompting, use `--force` flag.
	
List existing virtual machines
------------------------------

	list [--running] [MACHINE_NAME]
	
List all existing machines or a detailed info about given machine.
If `--running` is specified, only running machines are listed.
If MACHINE_NAME is specified, detailed information about a machine is displayed.

Machine details contain the following fields:
- cpus: how many assigned CPUs the machine has
- memory: how much memory (in MB)
- disk: how big the disk is (in MB)
- executionCap: hypervisor's execution capability
- cernvmVersion: used CernVM version
- apiPort: VM's port connected to the host OS
- baseFolder: where all the VM files are stored
- rdpPort: RDP port for accessing the machine
	
Pause a virtual machine
-----------------------

	pause MACHINE_NAME
	
Pause a running machine.
	
SSH into a machine
------------------

	ssh MACHINE_NAME
	
SSH into an existing machine. You must have ssh installed and available in your PATH. Not supported on Windows.
	
Start a virtual machine
-----------------------

	start MACHINE_NAME
	
Start an existing machine.

Stop a virtual machine
----------------------

	stop MACHINE_NAME
	
Stops a running machine. It saves the state, does not power off the machine.


Config files
============

In CernVM-Launch, you have two types of configs. A global CernVM-Launch config, that also affects CernVM-Launch behaviour, and a machine creation config, that provides parameters for machine creation (see above).


Global config file
------------------

Global config file has to be named `.cernvm-launch.conf` and placed in the user home directory, i.e.:

On Linux and Mac:

    ~/.cernvm-launch.conf
On Windows:
    
    ${USERPROFILE}\.cernvm-launch.conf

When specifying `launchHomeFolder` and `sharedFolder`, you must use **canonical paths**.
For the moment, `launchHomeFolder` is not supported on Mac OS.

Config file example with all recognized items:

    ########### CernVM-Launch configuration ###########
    # Folder on the host OS which will be shared to VMs
    sharedFolder=
    # Base folder on the host OS where a CernVM subdirectory ('CernVM' on Win and Mac, '.cernvm' on Linux) is created.
    # All VM configuration files and images are stored in this subdirectory (can get large).
    # Changing this folder will disconnect already existing machines from CernVM-Launch
    launchHomeFolder=
    ########### Default VM parameters ###########
    # VM's port connected to the host OS. Use 22 to have SSH access to the machine
    apiPort=22
    cernvmVersion=latest
    cpus=1
    memory=2048
    disk=20000
    executionCap=100
    # Flags: 64bit, headful mode, graphical extensions
    flags=49


Known issues
============

If you encounter a problem with creating symlinks in the shared folder from the host OS, please restart VirtualBox.
