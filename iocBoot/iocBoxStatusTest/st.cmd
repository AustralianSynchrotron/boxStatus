#!../../bin/linux-x86_64/BoxStatusTest
#
# $File: //ASP/tec/epics/boxStatus/trunk/iocBoot/iocBoxStatusTest/st.cmd $
# $Revision: #2 $
# $DateTime: 2023/05/13 14:49:10 $
# Last checked in by: $Author: starritt $
#

## You may have to change Box_Status_Test to something else
## everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/BoxStatusTest.dbd"
BoxStatusTest_registerRecordDeviceDriver pdbbase

## Load record instances
#dbLoadRecords("db/xxx.db","user=starrittHost")

dbLoadTemplate ("db/test.substitutions")

cd "${TOP}/iocBoot/${IOC}"
iocInit
  
dbl
  
# end
