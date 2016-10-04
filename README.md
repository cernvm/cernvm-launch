CernVM-Launch
=============

CernVM command line utility to control VirtualBox CernVMs.

Requirements
============

- VirtualBox has to be installed in a default location.

Usage
=====

CernVM-Launch supports the following operations.


Create a virtual machine
------------------------

	create [--name MACHINE_NAME] [--no-start] [--memory NUM] [--disk NUM] [--cpus NUM] USER_DATA_FILE [CONFIGURATION_FILE]
		
Create a machine with specified user (contextualization) data. By default, the machine is started right away (specify '\-\-no-start' to suppress that).

When creating a machine, you may specify the parameters in several ways: via command line arguments, via CONFIGURATION FILE or via global CernVM-Launch config file. Parameters are considered in the following precedence: 

Command line parameters > configuration file > global config > hardcoded defaults.

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
    diskURL=
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
There is one special parameter called 'flags', which uses a bit format for configuring several VM options.

    Active bit   Effect
      1          The system is 64-bit instead of 32-bit
      2          Use regular deployment (HDD) instead of micro-iso
      4          Include a guest additions CD-ROM
      8          Use floppyIO instead of contextualization CD-ROM
     16          Start the VM in headful mode
     32          Enable graphical extension (like drag-n-drop)
     64          Use secondary adapter instead of creating a NAT rule on the first one
    128          Use ttyS0 as external logfile.



Destroy an existing VM
-----------------------

	destroy [--force] MACHINE_NAME
	
Destroy an existing VM. By default, a running machine is not destroyed: you have to use '\-\-force' flag to do that.
	
List existing virtual machines
------------------------------

	list [--running] [MACHINE_NAME]
	
List all existing machines or a detailed info about given machine.
If '\-\-running' is specified, only running machines are listed.
If MACHINE_NAME is specified, detailed information about a machine is displayed.
	
Pause a virtual machine
-----------------------

	pause MACHINE_NAME
	
Pause a running machine.
	
SSH into a machine
------------------

	ssh MACHINE_NAME
	
SSH into an existing machine. It is not supported on Windows.
	
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

Global config file has to be named '.cernvm-launch.conf' and placed in the user home directory, i.e.:

On Linux:

    ~/.cernvm-launch.conf
On Windows:
    
    ${USERPROFILE}\.cernvm-launch.conf

Config file example with all recognized items:

    ########### CernVM-Launch configuration ###########
    # Folder on the host OS which will be shared to VMs
    sharedFolder=
    # Folder on the host OS where all VMs configuration files and images are stored (can get large)
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
