#!../../bin/linux-x86_64/BoxStatusTest
#
# $File: //ASP/tec/epics/boxStatus/trunk/iocBoot/iocBoxStatusTest/st.cmd $
# $Revision: #1 $
# $DateTime: 2016/05/14 18:21:38 $
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

var devBoxStatusDebug 4

cd "${TOP}/iocBoot/${IOC}"
iocInit
  
dbl
 
# end
