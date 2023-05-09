/* $File: //ASP/tec/epics/boxStatus/trunk/BoxStatusSup/src/devBoxStatus.c $
 * $Revision: #1 $
 * $DateTime: 2016/05/14 18:21:38 $
 * Last checked in by: $Author: starritt $
 *
 * Description
 * This module provides an device driver to support pinging a remote host and
 * collecting the result of the ping. The ping itself is performed by forking a
 * child process which execvp's (executes) ping, as:
 *
 *    ping -c 1 -W 3 <remote_hostname>
 *
 * The module waits for the child process to complete, and the exit code of the
 * ping child process determines the value assigned to the associated mbbi
 * record. Exit codes and meanings:
 *
 *    0   okay
 *    1   no reply and
 *    2   invalid/unknown hostname.
 *
 * The module kills the child process if it does not complete within 4s.
 *
 * The mbbi records may scan or I/O interrupt. The ping management functionality
 * is run aynchronessly using an internal thread. An I/O interrupt is issued at
 * the end of each ping cycle for the remote host. The read process mearly access
 * the latest known status - it does not instigate any processing per se.
 *
 * Note: this unit developed on and for executing on Linux.
 *
 * Copyright (c) 2014  Australian Synchrotron
 *
 * This module is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This module is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this module.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contact details:
 * andrew.starritt@synchrotron.org.au
 * 800 Blackburn Road, Clayton, Victoria 3168, Australia.
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

#include <alarm.h>
#include <cantProceed.h>
#include <dbScan.h>
#include <devSup.h>
#include <ellLib.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <errlog.h>
#include <mbbiRecord.h>

//
//
#define DEV_NAME       "Box Status"
#define DEV_VERSION    "1.1.2"
#define MAX_CHILDREN   10

static const double pollingInterval = 20.0;
static const double threadDelay     = 0.1;

static int devBoxStatusDebug = 0;
epicsExportAddress (int, devBoxStatusDebug);

//------------------------------------------------------------------------------
//
static void devprintf (const int required_min_verbosity,
                       const char* function,
                       const int line_no,
                       const char* format, ...)
{
   if (devBoxStatusDebug >= required_min_verbosity) {
      char message [200];
      va_list arguments;
      va_start (arguments, format);
      vsnprintf (message, sizeof (message), format, arguments);
      va_end (arguments);
      errlogPrintf ("BoxStatus: %3d:%s %s\n", line_no, function, message);
   }
}

// Wrapper macros to devprintf.
//
#define ERROR(...)    devprintf (0, __FUNCTION__, __LINE__, __VA_ARGS__);
#define WARNING(...)  devprintf (1, __FUNCTION__, __LINE__, __VA_ARGS__);
#define INFO(...)     devprintf (2, __FUNCTION__, __LINE__, __VA_ARGS__);
#define DETAIL(...)   devprintf (3, __FUNCTION__, __LINE__, __VA_ARGS__);

#define ASSERT(condtion, ...)   {                                           \
   if (!(condtion)) {                                                       \
      INFO (__VA_ARGS__);                                                   \
      return asynError;                                                     \
   }                                                                        \
}


#define ASSERT_INITIALISED   {                                              \
   if (this->readyToGo != true) {                                           \
      ERROR ("Not initalised\n");                                           \
      return asynError;                                                     \
   }                                                                        \
}


//------------------------------------------------------------------------------
// Forward declartions.
//
static long init_mbbi (int phase);
static long init_record_mbbi (mbbiRecord* prec);
static long get_mbbi_ioint_info (int cmd, mbbiRecord* prec, IOSCANPVT * scanpvt);
static long read_mbbi (mbbiRecord* prec);

// Device support set.
//
struct Device_Driver_Interface {
   long number;
   DEVSUPFUN report;
   DEVSUPFUN init;
   DEVSUPFUN init_record;
   DEVSUPFUN get_ioint_info;
   DEVSUPFUN read;
   DEVSUPFUN special_linconv;
};

static struct Device_Driver_Interface devMbbiBoxStatus = {
   5,
   NULL,
   init_mbbi,
   init_record_mbbi,
   get_mbbi_ioint_info,
   read_mbbi,
   NULL
};

epicsExportAddress (dset, devMbbiBoxStatus);

//------------------------------------------------------------------------------
// Own types
//
typedef enum eHostStatus {
   Unknown = 0,
   Okay,
   NoReply,
   Invalid,
   Timeout
} HostStatus;


typedef enum eProcessStatus {
   Pending = 0,
   Ready,
   Resoursed,
   Active,
   Terminating,
   Kill,
   Complete
} ProcessStatus;


// For diagnostic report only.
//
static const char* processStateName [7] =
   { "Pending", "Ready", "Resoursed",
     "Active", "Terminating", "Kill",
     "Complete" };


typedef struct sDriverRecordData {
   ELLNODE node;
   IOSCANPVT ioscanpvt;
   mbbiRecord* prec;
   char* remote_hostname;
   pid_t pid;          // associated process id (or 0)
   int ping_interval;  // expressed in thread cycle counts ~ threadDelay seconds
   int countdown;      // expressed in thread cycle counts
   ProcessStatus processStatus;
   HostStatus hostStatus;
} DriverRecordData;

typedef DriverRecordData *DriverRecordDataPtr;


//------------------------------------------------------------------------------
//
static epicsMutexId linked_list_mutex = NULL;
static ELLLIST item_list = ELLLIST_INIT;
static epicsThreadId thread_id = NULL;
static int epicsRunning = 0;
static int activeItemsCount = 0;


//------------------------------------------------------------------------------
// Starts the ping child process.
// Returns fork result (if parent)
//
static pid_t activatePing (DriverRecordDataPtr pDRD)
{
   pid_t pid;

   // Create a child process to do the actual ping.
   //
   pid = fork ();
   if (pid == 0) {
      // We are the child process....
      //
      int maxfd;
      int fd;
      int save1;
      int save2;
      int status;
      char* pargs [7];
      char* file;

      // This is the child process - first close all unwanted file descriptors.
      //
      maxfd = sysconf(_SC_OPEN_MAX);
      if (maxfd > 8192) maxfd = 8192;    // sanity check
      for (fd = 3; fd < maxfd; fd++) {
         close (fd);
      }

      // Send output to /dev/null
      //
      save1 = dup (1);
      save2 = dup (2);
      fd = open ("/dev/null", O_WRONLY);
      status = dup2 (fd, 1);
      status = dup2 (fd, 2);

      // Set up ping arguments.
      //
      pargs [0] = "ping";
      pargs [1] = "-c";
      pargs [2] = "1";
      pargs [3] = "-W";
      pargs [4] = "3";
      pargs [5] = pDRD->remote_hostname;
      pargs [6] = NULL;
      file = pargs [0];

      INFO ("Calling execvp (%s, %s)", file, pargs [5]);
      status = execvp (file, pargs);

      // Hopefully we never get here, but if we do, report error and exit process.
      //
      status = dup2 (save1, 1);
      status = dup2 (save2, 2);
      perror ("execvp (ping, ...) failed");
      _exit (2);

   } else if (pid > 0) {
      // We are the parent, all went well.
      //
   } else {
      // Still parent, but something went wrong.
      //
      perror ("box status: fork (ping, ...) failed");
   }

   return pid;
}

//------------------------------------------------------------------------------
// This function provides the state machine to create/control/monitor/terminate
// child ping process.
//
static void progressItem (DriverRecordDataPtr pDRD)
{
   ProcessStatus ps;
   pid_t pid;
   int waitpid_code;
   int status;
   int exit_code;
   int kill_code;

   ps = pDRD->processStatus;
   if (ps != Pending) {
      INFO ("processing %-12s in state %-12s, active: %d", pDRD->remote_hostname,
            processStateName [pDRD->processStatus], activeItemsCount);
   }

   switch (pDRD->processStatus) {

     case Pending:
         if (pDRD->countdown > 0) {
            pDRD->countdown--;
         } else {
            pDRD->processStatus = Ready;
         }
         break;

      case Ready:
         if (activeItemsCount < MAX_CHILDREN) {
            pDRD->processStatus = Resoursed;
         }
         break;

      case Resoursed:
         pid = activatePing (pDRD);
         if (pid >= 0) {
            activeItemsCount++;
            pDRD->pid = pid;
            pDRD->processStatus = Active;
            pDRD->countdown = (int)(4.0/threadDelay);   // ~ 4 seconds
         } else {
            // Just try again.
            //
            pDRD->processStatus = Complete;
         }
         break;

      case Active:
         activeItemsCount++;
         waitpid_code = waitpid (pDRD->pid, &status, WNOHANG);
         if (waitpid_code == pDRD->pid) {
            // All done - recover exit code.
            //
            exit_code = status / 256;
            pDRD->hostStatus = (HostStatus) (exit_code + 1);  // map exit code to status
            pDRD->processStatus = Complete;
            printf ("*** %s : exit code %d\n", pDRD->remote_hostname, exit_code);

         } else if (pDRD->countdown > 0) {
            // Still waiting - just decrement counter.
            //
            pDRD->countdown--;

         } else {
            pDRD->hostStatus = Timeout;
            kill_code = kill (pDRD->pid, SIGTERM);

            pDRD->processStatus = Terminating;
            pDRD->countdown = 10;    // approx 1 second
         }
         break;

      case Terminating:
         activeItemsCount++;
         if (pDRD->countdown > 0) {
            pDRD->countdown--;
            waitpid_code = waitpid (pDRD->pid, &status, WNOHANG);
            if (waitpid_code == pDRD->pid) {
               // Process terminated.
               //
               pDRD->processStatus = Complete;
            }
         } else {
            pDRD->processStatus = Kill;
         }
         break;

      case Kill:
         // No more chances. Kill child process.  We must wait for the child
         // process - or system leaves a zombie process, which eventually will
         // accumulate and fill the pid list.
         //
         kill_code = kill (pDRD->pid, SIGKILL);
         waitpid_code = waitpid (pDRD->pid, &status, 0);
         pDRD->pid = 0;
         pDRD->processStatus = Complete;
         break;

      case Complete:
         // All done - start again.
         //
         pDRD->processStatus = Pending;
         pDRD->countdown = pDRD->ping_interval;
         break;


      default:
         printf ("%s: invalid state %d\n", pDRD->remote_hostname, (int) pDRD->processStatus);
         pDRD->processStatus = Complete;
         break;
   }

   if (pDRD->processStatus != ps) {
      INFO ("processed  %-12s to state %-12s, ac: %d", pDRD->remote_hostname,
            processStateName [pDRD->processStatus], activeItemsCount);
   }
}

//------------------------------------------------------------------------------
// This function runs in seperate thread.
// We use one thread to manage the pinging of all boxes.
//
static void *run_in_background (void* dummy)
{
   ELLLIST temp_list = ELLLIST_INIT;
   DriverRecordDataPtr pDRD = NULL;
   DriverRecordDataPtr pnext = NULL;

   printf ("%s: thread created \n", DEV_NAME);
   epicsThreadSleep (2.0);
   printf ("\n%s: thread starting \n", DEV_NAME);

   ellInit (&temp_list);
   while (epicsRunning == 1) {
      epicsMutexLock (linked_list_mutex);

      // Process items in the pending list - are they ready to be re-run?
      //
      activeItemsCount = 0;
      pDRD = (DriverRecordDataPtr) ellFirst (&item_list);
      while (pDRD) {
         // As we might remove this item from the list, so we take note of the
         // next node now as opposed to the end of the loop.
         //
         pnext = (DriverRecordDataPtr) ellNext (&pDRD->node);

         progressItem (pDRD);
         if (pDRD->processStatus == Complete) {
            // Has completed - move to end of list to await next turn
            // This is important if more items to process than allowed resourse.
            //
            ellExtract (&item_list, (ELLNODE*) pDRD, (ELLNODE*) pDRD, &temp_list);

            // Call EPICS to trigger record if using I/O Intr.
            //
            if (pDRD->ioscanpvt) {
               scanIoRequest (pDRD->ioscanpvt);
            }
         }

         pDRD = pnext;  // Iterate to next node
      }

      // Now append all completed items to end of the list.
      // This will becone Pending next time processed.
      //
      ellConcat (&item_list, &temp_list);

      // All done for this cycle - release mutex.
      //
      epicsMutexUnlock (linked_list_mutex);

      // Small delay, then go round again.
      //
      epicsThreadSleep (threadDelay);
   }

   // Shutting down - kill any child processes.
   //
   printf ("\n%s: thread shutting down\n", DEV_NAME);

   pDRD = (DriverRecordDataPtr) ellFirst (&item_list);
   while (pDRD) {
      pnext = (DriverRecordDataPtr) ellNext (&pDRD->node);
      if (pDRD->pid > 0) {
         int status;
         kill (pDRD->pid, SIGKILL);
         waitpid (pDRD->pid, &status, 0);
      }
      pDRD = pnext;
   }

   printf ("%s: thread complete\n", DEV_NAME);

   return 0;
}

//------------------------------------------------------------------------------
//
static void finalise (void* dummy)
{
   epicsRunning = 0;
}

//------------------------------------------------------------------------------
//
static void initialise ()
{
   printf ("%s: version %s\n", DEV_NAME, DEV_VERSION);

   linked_list_mutex = epicsMutexCreate ();
   ellInit (&item_list);

   /* Start thread
    */
   epicsRunning = 1;
   thread_id = epicsThreadCreate
         ("box_status", epicsThreadPriorityMedium,
          epicsThreadGetStackSize (epicsThreadStackMedium),
          (EPICSTHREADFUNC) run_in_background, NULL);

   // Register call back at EPICS shutdown.
   //
   epicsAtExit (finalise, NULL);
}

//------------------------------------------------------------------------------
// Initialise driver function.
// Note: init_mbbi does all initialisation - no registrar nor ioc shell command required.
//
static long init_mbbi (int phase)
{
   if (phase == 0) {
      initialise ();
   }
   return 0;
}

/*------------------------------------------------------------------------------
 */
static long init_record_mbbi (mbbiRecord* prec)
{
   DriverRecordDataPtr pDRD = NULL;
   char* hostname;

   prec->dpvt = NULL;
   if (prec->inp.type != INST_IO) {
      printf ("%s:%s invalid INP link type\n", DEV_NAME, prec->name);
      return (-1);
   }

   hostname = prec->inp.value.instio.string;
   if (hostname == NULL || strlen (hostname) == 0) {
      printf ("%s:%s invalid/null remote host\n", DEV_NAME, prec->name);
      return (-1);
   }

   // Allocate structure for this record.
   //
   pDRD = (DriverRecordDataPtr) mallocMustSucceed (sizeof (DriverRecordData),
                                                   "box_status::init_record_mbbi");

   // Initialise driver record data for this record.
   //
   scanIoInit (&pDRD->ioscanpvt);
   pDRD->prec = prec;
   pDRD->remote_hostname = epicsStrDup (hostname);
   // approx 20 seconds - fixed for now, maybe extract from INP field.
   pDRD->ping_interval = (int) (pollingInterval / threadDelay);

   INFO ("ping_interval %d", pDRD->ping_interval);

   // Don't start all at same time - spread out the "lurve".
   //
   static int phase = 0;
   phase += 17;               // 17 and 200 are relatively prime
   pDRD->countdown = phase % 200;
   pDRD->pid = 0;
   pDRD->hostStatus = Unknown;
   pDRD->processStatus = Pending;

   prec->dpvt = (void*) pDRD;

   // Lastly add to the pending list.
   //
   epicsMutexLock (linked_list_mutex);
   ellAdd (&item_list, (ELLNODE *) pDRD);
   epicsMutexUnlock (linked_list_mutex);

   return 0;
}

//------------------------------------------------------------------------------
//
static long get_mbbi_ioint_info (int cmd, mbbiRecord* prec, IOSCANPVT * scanpvt)
{
   DriverRecordDataPtr pDRD = (DriverRecordDataPtr) prec->dpvt;

   // Did record fail to initialise?
   //
   if (pDRD == NULL) {
      return (-1);
   }

   *scanpvt = pDRD->ioscanpvt;
   return 0;
}

//------------------------------------------------------------------------------
//
static long read_mbbi (mbbiRecord* prec)
{
   DriverRecordDataPtr pDRD = (DriverRecordDataPtr) prec->dpvt;

   // Did record fail to initialise?
   //
   if (pDRD == NULL) {
      return (-1);
   }

   // No mutex - just read the latest available value.
   //
   prec->rval = (epicsEnum16) pDRD->hostStatus;
   prec->nsev = NO_ALARM;
   prec->nsta = NO_ALARM;
   prec->udf = 0;

   return 0;
}

// end
