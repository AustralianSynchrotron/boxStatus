# Box Status Module

## Introduction

The purpose of the box status module is to allow an EPICS IOC to check if any
network capable box (device) is connected/online, essentially by pinging the box.
Any box that can respond to a ping is suitable, e.g. EPICS IOC hosts, MOXA
terminal servers, network switchs, motion controllers, etc.

This module provides an device driver to support pinging a remote host and
collecting the result of the ping.
The ping itself is performed by forking a child process which execvp's (executes)
ping, as:

    ping -c 1 -W 3 <remote_hostname>

The module waits for the child process to complete, and the exit code of the
ping child process determines the value assigned to the associated mbbi
record.
Ping exit codes and meanings:

   0   okay
   1   no reply and
   2   invalid/unknown hostname.

The module kills the child process if it does not complete within 4 seconds.

The mbbi records may scan or I/O interrupt.
The ping management functionality is run aynchronessly using an internal thread.
An I/O interrupt is issued at the end of each ping cycle for the remote host.
The read process mearly access the latest known status - it does not instigate
any processing per se.

__Note:__ this unit developed on and for executing on Linux.

__Note:__ ping must be on the PATH (/usr/bin/ping on CentOS 7) and have the run
as root "sticky" bit set.
If needs be, this can be set as follows:

    sudo chmod a+s /usr/bin/ping

## EPICS Database template

The module includes Box_Status.template which can be used as is, or as a guide
to creating your template file.
As well as the box (device) status itself, the template also includes device
function and device location stringin records.

The mbbi status record reflects the status of the ping.
The status are:

| State    | Severity | Comment          |
|:---------|:---------|:-----------------|
| Unknown  | INVALID  | Undefined        |
| Ok       | NO_ALARM | ping exit code 0 |
| No reply | MAJOR    | ping exit code 1 |
| Invalid  | MAJOR    | ping exit code 2 |
| Timeout  | MAJOR    | ping timeout     |


__Note:__ In the template the BOX macro paramters is used for both the record
name and the device to be pinged defined in thr INP field.
This must be a basic host name, not an IP address nor a fully qualified domain
name (record names can contain dots ('.')).

## Including the Box Status module into an EPICS IOC

### general

Within the EPICS IOC's configure/RELEASE file (or an included file such as
RELEASE.local) add:

    BOXSTATUS=**the location of the boxStatus module in your environment**

Within the EPICS IOC's make file (&lt;top&gt;/IOC_Name_App/src/Makefile), add:

    IOC_Name_DBD  += devBoxStatus.dbd
    IOC_Name_LIBS += devBoxStatus

And within the IOC's st.cmd file (example):

    dbLoadRecords ("$(BOXSTATUS)/Box_Status.template", "BOX=SR16MB03DEV01, FUNCTION=Primary Widget, LOCATION=Sever Room 2")
    dbLoadRecords ("$(BOXSTATUS)/Box_Status.template", "BOX=SR16MB03DEV02, FUNCTION=Secondary Widget, LOCATION=Tunnel Roof")

__Note:__ It is probably better to use a substitution file if more than a limited
number of devices need to be specified.


### bundle build

As above, except add the following to the EPICS IOC's configure/RELEASE file:

    BOXSTATUS=$(BUNDLE)/epics/boxStatus

### easy build

Within the EPICS IOC's build_instructions file add:

    comp=/tec/epics/boxStatus/trunk/BoxStatusSup

Within the EPICS IOC's make file (&lt;top&gt;/IOC_Name_App/src/Makefile), as above.

And within the IOC's st.cmd file (example):

    dbLoadRecords ("db/Box_Status.template", "BOX=SR16MB03DEV01, FUNCTION=Primary Widget, LOCATION=Sever Room 2")
    dbLoadRecords ("db/Box_Status.template", "BOX=SR16MB03DEV02, FUNCTION=Secondary Widget, LOCATION=Tunnel Roof")


## Release Notes

### R1.2.4

Documentation and licence update for sharing on GitHub.
Removed obsolete archive meta comments.

### R1.2.3

Fixed typo in BoxStatusSup/opi/Makefile

### R1.2.2

Added EPICS Qt box_status.ui file.

### R1.2.1

Updated top Makefile file.

### R1.1.2

Added perforce key words to configure/RELEASE.

### R1.1.1

Initial delivery


<font size="-1">Last updated: Thu Jan 30 17:26:12 2025</font>
<br>
