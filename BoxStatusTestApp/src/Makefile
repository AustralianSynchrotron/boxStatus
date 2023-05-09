# $File: //ASP/tec/epics/boxStatus/trunk/BoxStatusTestApp/src/Makefile $
# $Revision: #1 $
# $DateTime: 2016/05/14 18:21:38 $
# Last checked in by: $Author: starritt $
#
TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = BoxStatusTest
# BoxStatusTest.dbd will be created and installed
DBD += BoxStatusTest.dbd

# BoxStatusTest.dbd will be made up from these files:
BoxStatusTest_DBD += base.dbd

# Include dbd files from all support applications:
#
BoxStatusTest_DBD += devBoxStatus.dbd

# Add all the support libraries needed by this IOC
#
BoxStatusTest_LIBS += devBoxStatus

# BoxStatusTest_registerRecordDeviceDriver.cpp derives from BoxStatusTest.dbd
BoxStatusTest_SRCS += BoxStatusTest_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
BoxStatusTest_SRCS_DEFAULT += BoxStatusTestMain.cpp
BoxStatusTest_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#BoxStatusTest_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
BoxStatusTest_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# end
