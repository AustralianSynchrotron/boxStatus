# $File: //ASP/tec/epics/boxStatus/trunk/BoxStatusSup/src/Makefile $
# $Revision: #1 $
# $DateTime: 2016/05/14 18:21:38 $
# Last checked in by: $Author: starritt $
#
TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#========================================

#========================================
# build octet support library
#
LIBRARY_IOC += devBoxStatus

# Library Source files
#
devBoxStatus_SRCS += devBoxStatus.c

# Link with the base library
#
devBoxStatus_LIBS += $(EPICS_BASE_IOC_LIBS)

# Install .dbd and .db files
#
DBD += devBoxStatus.dbd

#========================================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

# end
