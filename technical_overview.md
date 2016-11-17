Technical overview
==================

CernVM-Launch comprises of two parts: `libcernvm` and `Launch`.

libcernvm 
=========

The core for interacting with VirtualBox, originally developed by Ioannis Charalampidis
for CernVM Online. It is a wrapper for VirtualBox manage utility (VBoxManage), a command
line tool for interaction with VirtualBox.

It encapsulates VBoxManage commands and workflows, that are common for CernVM instantiation.
Workflow example could be a VM creation: it is needed to create a VM instance, configure its
storage, network, port forwarding, shared folder, etc.

All VM interaction is encapsulated into a *session* (not to be mistaken with a web session).
This session tracks the VM state via state machine and session configuration file. This file
contains all information about the machine: provided by both VirtualBox and user. 

CernVM creation
---------------

When a CernVM is created, standard procedure is to add a `ucernvm` image as a bootable source
to the machine. If needed, `libcernvm` downloads this file for you. You can specify the
`ucernvm` version in creation parameters.

A user can provide *user data* (called *context* in CernVM terminology) in a text
file. This file is then converted into an ISO image and mounted to the VM. This ISO image
creation is a part of `libcernvm`. It has a hardcoded byte layout of the resulting image and
it appends/replaces its variable parts: file sizes and file contents.

The resulting ISO image layout is following:
    
    /
    ├── ec2
    │   └── latest
    │       ├── meta-data.json
    │       └── user-data
    ├── openstack
    │   └── latest
    │       ├── meta_data.json
    │       └── user_data
    └── readme


`user-data` and `user_data` files are identical: they are only placed in different locations
in order to be found by all contextualization clients.

### VirtualBox creation commands
Following VirtualBox VBoxManage commands are roughly equivalent to a standard CernVM creation.

    # Create a VM
    VBoxManage create --name NAME --ostype Linux26_64 --basefolder FOLDER --register
    # Add IDE controller
    VBoxManage storagectl MACHINE_NAME --name IDE --add ide
    # Add SATA controller
    VBoxManage storagectl MACHINE_NAME --name SATA --add sata
    # Add floppy controller
    VBoxManage storagectl MACHINE_NAME --name Floppy --add floppy
    # Configure basic parameters
    VBoxManage modifyvm MACHINE_NAME --cpus N --memory N --vram N --acpi on --ioapic on
    # Configure VRDE
    VBoxManage modifyvm MACHINE_NAME --vrde on --vrdeaddress 127.0.0.1 --vrdeauthtype null --vrdemulticon on --vrdeport N
    # Set boot medium (type 'disk' for other deployments)
    VBoxManage modifyvm MACHINE_NAME --boot dvd
    # Enable NIC 1 (NAT)
    VBoxManage modifyvm MACHINE_NAME --nic1 nat --nictype1 virtio --natdnshostresolver1 on
    # Configure NIC (NAT) rule (replace PORT and FORWARDED_PORT as desired)
    VBoxManage modifyvm MACHINE_NAME --natpf1 guestapi,tcp,127.0.0.1,PORT,,FORWARDED_PORT
    # Enable 2. NIC (hostonly)
    VBoxManage modifyvm MACHINE_NAME --nic2 hostonly --nictype2 virtio --hostonlyadapter2 ifHO
    # Set graphical additions
    VBoxManage modifyvm MACHINE_NAME --draganddrop bidirectional --clipboard bidirectional
    # Set up shared folder
    VBoxManage sharedfolder add MACHINE_NAME --name SHARED_FOLDER_NAME --hostpath HOST_PATH --automount
    # Enable symlink creation from the guest OS through the shared folder
    VBoxManage setextradata MACHINE_NAME VBoxInternal2/SharedFoldersEnableSymlinksCreate/ SHARED_FOLDER_NAME 1


Runtime data
------------

`libcernvm` uses several directories for storing its data. The root directory is located in
a different place for each platform:

    Windows: %APPDATA%/CernVM
    Mac OS:  $HOME/Library/Application Support/CernVM
    Linux:   $HOME/.cernvm

In this root directory a following layout is created:

    /
    ├── cache/
    ├── config/
    └── run/

All downloaded `ucernvm` images are stored in the `cache` directory. Run files (e.g. VBox
images, session files) are stored in `run`. The directory `config` is not used in our
use case (it is used for `libcernvm` configuration).


Launch
======

Launch part provides user friendly interaction with `libcernvm`, handling user parameters
and invoking appropriate `libcernvm` functionality. Even though `libcernvm` supports
a VM state machine (it can track the actual state of a VM, e.g. created, running), `Launch`
treats it statelessly, in order to avoid a requirement of a running daemon.
Every time you issue a `Launch` command, `libcernvm` performs its initialization.
