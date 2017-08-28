// exception.cc
//  Entry point into the Nachos kernel from user programs.
//  There are two kinds of things that can cause control to
//  transfer back to here from user code:
//
//  syscall -- The user code explicitly requests to call a procedure
//  in the Nachos kernel.  Right now, the only function we support is
//  "Halt".
//
//  exceptions -- The user code does something that the CPU can't handle.
//  For instance, accessing memory that doesn't exist, arithmetic errors,
//  etc.
//
//  Interrupts (which can also cause control to transfer from user
//  code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"
#include "machine.h"
#include "disk.h"
#include "addrspace.h"
//----------------------------------------------------------------------
// ExceptionHandler
//  Entry point into the Nachos kernel.  Called when a user program
//  is executing, and either does a syscall, or generates an addressing
//  or arithmetic exception.
//
//  For system calls, the following is the calling convention:
//
//  system call code -- r2
//    arg1 -- r4
//    arg2 -- r5
//    arg3 -- r6
//    arg4 -- r7
//
//  The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//  "which" is the kind of exception.  The list of possible exceptions
//  are in machine.h.
//----------------------------------------------------------------------
static Semaphore* readAvail;
static Semaphore* writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

static void ConvertIntToHex(unsigned v, Console* console)
{
    unsigned x;
    if (v == 0)
        return;
    ConvertIntToHex(v / 16, console);
    x = v % 16;
    if (x < 10) {
        writeDone->P();
        console->PutChar('0' + x);
    }
    else {
        writeDone->P();
        console->PutChar('a' + x - 10);
    }
}

void ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp;
    unsigned printvalus; // Used for printing in hex
    if (!initializedConsoleSemaphores) {
        readAvail = new Semaphore("read avail", 0);
        writeDone = new Semaphore("write done", 1);
        initializedConsoleSemaphores = true;
    }
    Console* console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);
    ;

    if ((which == SyscallException) && (type == SysCall_Halt)) {
        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SysCall_PrintInt)) {
        printval = machine->ReadRegister(4);
        if (printval == 0) {
            writeDone->P();
            console->PutChar('0');
        }
        else {
            if (printval < 0) {
                writeDone->P();
                console->PutChar('-');
                printval = -printval;
            }
            tempval = printval;
            exp = 1;
            while (tempval != 0) {
                tempval = tempval / 10;
                exp = exp * 10;
            }
            exp = exp / 10;
            while (exp > 0) {
                writeDone->P();
                console->PutChar('0' + (printval / exp));
                printval = printval % exp;
                exp = exp / 10;
            }
        }
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintChar)) {
        writeDone->P();
        console->PutChar(machine->ReadRegister(4)); // echo it!
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintString)) {
        vaddr = machine->ReadRegister(4);
        machine->ReadMem(vaddr, 1, &memval);
        while ((*(char*)&memval) != '\0') {
            writeDone->P();
            console->PutChar(*(char*)&memval);
            vaddr++;
            machine->ReadMem(vaddr, 1, &memval);
        }
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else if ((which == SyscallException) && (type == SysCall_PrintIntHex)) {
        printvalus = (unsigned)machine->ReadRegister(4);
        writeDone->P();
        console->PutChar('0');
        writeDone->P();
        console->PutChar('x');
        if (printvalus == 0) {
            writeDone->P();
            console->PutChar('0');
        }
        else {
            ConvertIntToHex(printvalus, console);
        }
        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else if ((which == SyscallException) && (type == SysCall_GetReg)) {
        //Implemented By Siddhartha

        int reg_to_read = (unsigned)machine->ReadRegister(4);
        int value = (unsigned)machine->ReadRegister(reg_to_read);

        machine->WriteRegister(2, value);

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }

    else if ((which == SyscallException) && (type == SysCall_GetPA)) {
        //Implemented By Siddhartha

        int i;
        unsigned int virtualPageNumber, offset;
        int virtAddr = machine->ReadRegister(4);
        int physAddr;
        TranslationEntry* entry;
        unsigned int pageFrame;

        virtualPageNumber = (unsigned)virtAddr / PageSize;
        offset = (unsigned)virtAddr % PageSize;

        if (machine->tlb == NULL) {
            if (virtualPageNumber > machine->pageTableSize) {
                machine->WriteRegister(2, -1);
            }
            else if (!machine->KernelPageTable[virtualPageNumber].valid) {
                machine->WriteRegister(2, -1);
            }
            else {
                entry = &(machine->KernelPageTable[virtualPageNumber]);
                pageFrame = entry->physicalPage;
                if (pageFrame >= NumPhysPages) {
                    machine->WriteRegister(2, -1);
                }
                else {
                    physAddr = pageFrame * PageSize + offset;
                    machine->WriteRegister(2, physAddr);
                }
            }
        }
        else {
            for (entry = NULL, i = 0; i < TLBSize; i++)
                if ((machine->tlb)[i].valid && ((machine->tlb)[i].virtualPage == virtualPageNumber)) {
                    entry = &(machine->tlb)[i]; // FOUND!
                    break;
                }
            pageFrame = entry->physicalPage;
            if (pageFrame >= NumPhysPages) {
                machine->WriteRegister(2, -1);
            }
            else {
                physAddr = pageFrame * PageSize + offset;
                machine->WriteRegister(2, physAddr);
            }
        }

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else if ((which == SyscallException) && (type == SysCall_GetPID)) {
        //Implemented By Gurpreet
        machine->WriteRegister(2, currentThread->GetPID());

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else if ((which == SyscallException) && (type == SysCall_GetPPID)) {
        //Implemented By Gurpreet
        machine->WriteRegister(2, currentThread->GetPPID());  

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else if ((which == SyscallException) && (type == SysCall_Time)) {
        //Implemented By Gurpreet
        machine->WriteRegister(2, stats->totalTicks);

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else if ((which == SyscallException) && (type == SysCall_Yield)) {
        //Implemented By Shobhit
        currentThread->YieldCPU();

        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else if ((which == SyscallException) && (type == SysCall_Sleep)) {
        //Implemented By Gurpreet
        
        int sleepTime = machine->ReadRegister(4); //Read the sleep time

        if (sleepTime == 0) {
            currentThread->YieldCPU(); //Yield if sleep time is zero
        }
        else {
            IntStatus oldLevel = interrupt->SetLevel(IntOff); //Disable Interrupts

            waitingThreadList->SortedInsert((void *) currentThread, stats->totalTicks + sleepTime); //Add thread to waiting queue restored on every interrupt
            currentThread->PutThreadToSleep(); //Put the thread to sleep

            (void) interrupt->SetLevel(oldLevel); //Restore interrupts to previous state
        }


        // Advance program counters.
        machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
        machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
        machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
    }
    else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
