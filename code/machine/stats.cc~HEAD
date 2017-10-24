// stats.h 
//	Routines for managing statistics about Nachos performance.
//
// DO NOT CHANGE -- these stats are maintained by the machine emulation.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "stats.h"

//----------------------------------------------------------------------
// Statistics::Statistics
// 	Initialize performance metrics to zero, at system startup.
//----------------------------------------------------------------------

Statistics::Statistics() {
    totalTicks = idleTicks = systemTicks = userTicks = 0;
    numDiskReads = numDiskWrites = 0;
    numConsoleCharsRead = numConsoleCharsWritten = 0;
    numPageFaults = numPacketsSent = numPacketsRecvd = 0;
    totalBurstTicks = totalWaitTicks = 0;
    numCPUBursts=maxBurstTicks=0;
    minBurstTicks=1000;
}

//----------------------------------------------------------------------
// Statistics::Print
// 	Print performance metrics, when we've finished everything
//	at system shutdown.
//----------------------------------------------------------------------

void
Statistics::Print() {
    printf("Ticks: total %d, idle %d, system %d, user %d\n", totalTicks, idleTicks, systemTicks, userTicks);
    printf("Disk I/O: reads %d, writes %d\n", numDiskReads, numDiskWrites);
    printf("Console I/O: reads %d, writes %d\n", numConsoleCharsRead, numConsoleCharsWritten);
    printf("Paging: faults %d\n", numPageFaults);
    printf("Network I/O: packets received %d, sent %d\n", numPacketsRecvd, numPacketsSent);
    /* ======================= CUSTOM ======================= */
    printf("Ticks spent in CPU Burst: %d\n", totalBurstTicks);
    printf("Total number of CPU Bursts: %d\n", numCPUBursts);
    printf("Average CPU Burst: %d\n", (totalBurstTicks/numCPUBursts+1*(totalBurstTicks%numCPUBursts==0)));
    printf("Performance estimates(Not sure which one is correct\n");
    printf("using totalBurstTicks: %d\n", (int)(100*((double)totalBurstTicks/totalTicks)));
    printf("using idleTicks: %d\n", (int)(100-100*((double)idleTicks/totalTicks)));
    printf("Minimum CPU Burst Ticks: %d\n", minBurstTicks);
    printf("Maximum CPU Burst Ticks: %d\n", maxBurstTicks);
    printf("Ticks spent waiting in Queue: %d\n", totalWaitTicks);
    /* ======================= CUSTOM ======================= */
}
