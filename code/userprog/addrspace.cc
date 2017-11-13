// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void
SwapHeader(NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//	"executable" is the file containing the object code to load into memory
//
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(OpenFile *executable)
{
    progExecutable = executable;

    NoffHeader noffH;
    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;

    progExecutable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) &&
        (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;

    numVirtualPages = divRoundUp(size, PageSize);
    size = numVirtualPages * PageSize;
    // Backup for every thread
    backUpStore = new char[numVirtualPages*PageSize];

    ASSERT(numVirtualPages + numPagesAllocated <= NumPhysPages);

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
          numVirtualPages, size);

    KernelPageTable = new TranslationEntry[numVirtualPages];
    /* ------------------------ CUSTOM ------------------------ */
    if (pageReplaceAlgo == 0)
    {
        for (i = 0; i < numVirtualPages; i++)
        {
            KernelPageTable[i].virtualPage = i;
            KernelPageTable[i].physicalPage = i + numPagesAllocated;
            KernelPageTable[i].valid = TRUE;
            KernelPageTable[i].use = FALSE;
            KernelPageTable[i].dirty = FALSE;
            KernelPageTable[i].readOnly = FALSE;
            KernelPageTable[i].isBackedUp = FALSE;
        }

        bzero(&machine->mainMemory[numPagesAllocated * PageSize], size);

        numPagesAllocated += numVirtualPages;

        if (noffH.code.size > 0)
        {
            DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
                  noffH.code.virtualAddr, noffH.code.size);
            vpn = noffH.code.virtualAddr / PageSize;
            offset = noffH.code.virtualAddr % PageSize;
            entry = &KernelPageTable[vpn];
            pageFrame = entry->physicalPage;
            executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]), noffH.code.size, noffH.code.inFileAddr);
        }
        if (noffH.initData.size > 0)
        {
            DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
                  noffH.initData.virtualAddr, noffH.initData.size);
            vpn = noffH.initData.virtualAddr / PageSize;
            offset = noffH.initData.virtualAddr % PageSize;
            entry = &KernelPageTable[vpn];
            pageFrame = entry->physicalPage;
            executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]), noffH.initData.size, noffH.initData.inFileAddr);
        }
    }
    else
    {
        for (i = 0; i < numVirtualPages; i++)
        {
            KernelPageTable[i].virtualPage = i;
            KernelPageTable[i].physicalPage = -1;
            KernelPageTable[i].valid = FALSE;
            KernelPageTable[i].use = FALSE;
            KernelPageTable[i].dirty = FALSE;
            KernelPageTable[i].readOnly = FALSE;
            KernelPageTable[i].isBackedUp = FALSE;
        }
    }
    /* ------------------------ CUSTOM ------------------------ */
}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace (ProcessAddressSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(ProcessAddressSpace *parentSpace)
{
    fileName = parentSpace->fileName;
    progExecutable = fileSystem->Open(fileName);
    if (progExecutable == NULL)
    {
        printf("Unable to open file %s\n", fileName);
        ASSERT(false);
    }
    numVirtualPages = parentSpace->GetNumPages();
    unsigned i, j, size = numVirtualPages * PageSize;

    // Backup for every thread
    backUpStore = new char[numVirtualPages*PageSize];

    ASSERT(numVirtualPages + numPagesAllocated <= NumPhysPages);

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
          numVirtualPages, size);

    TranslationEntry *parentPageTable = parentSpace->GetPageTable();
    KernelPageTable = new TranslationEntry[numVirtualPages];

    unsigned startAddrParent, startAddrChild;

    /* ------------------------ CUSTOM ------------------------ */
    for (int i = 0; i < numVirtualPages; i++)
    {
        KernelPageTable[i].virtualPage = i;
        KernelPageTable[i].use = parentPageTable[i].use;
        KernelPageTable[i].readOnly = parentPageTable[i].readOnly;
        KernelPageTable[i].dirty = parentPageTable[i].dirty;
        KernelPageTable[i].shared = parentPageTable[i].shared;
        KernelPageTable[i].isBackedUp = parentPageTable[i].isBackedUp;

        if (parentPageTable[i].valid == TRUE && parentPageTable[i].shared == FALSE)
        {
            KernelPageTable[i].physicalPage = GetNextFreePage(parentPageTable[i].physicalPage);
            KernelPageTable[i].valid = TRUE;
            
            /* ------------------------ CUSTOM ------------------------ */
            InvertedPageTable[ KernelPageTable[i].physicalPage ].virtualPage = i;
            /* ------------------------ CUSTOM ------------------------ */

            startAddrParent = parentPageTable[i].physicalPage * PageSize;
            startAddrChild = KernelPageTable[i].physicalPage * PageSize;

            for (int j = 0; j < PageSize; j++)
            {
                machine->mainMemory[startAddrChild + j] = machine->mainMemory[startAddrParent + j];
            }

            currentThread->SortedInsertInWaitQueue(1000 + stats->totalTicks);
        }
        else
        {
            KernelPageTable[i].physicalPage = parentPageTable[i].physicalPage;
            KernelPageTable[i].valid = parentPageTable[i].valid;
        }
    }
    /* ------------------------ CUSTOM ------------------------ */
}

/* ------------------------ CUSTOM ------------------------ */
unsigned int ProcessAddressSpace::AllocateSharedMemory(int size)
{
    unsigned int numRequiredPages = numVirtualPages + divRoundUp(size, PageSize);

    ASSERT(numRequiredPages <= NumPhysPages)

    TranslationEntry *newPageTable = new TranslationEntry[numRequiredPages];

    unsigned int i;
    for (i = 0; i < numVirtualPages; i++)
    {
        newPageTable[i].virtualPage = KernelPageTable[i].virtualPage;
        newPageTable[i].physicalPage = KernelPageTable[i].physicalPage;
        newPageTable[i].valid = KernelPageTable[i].valid;
        newPageTable[i].use = KernelPageTable[i].use;
        newPageTable[i].dirty = KernelPageTable[i].dirty;
        newPageTable[i].readOnly = KernelPageTable[i].readOnly;
        newPageTable[i].shared = KernelPageTable[i].shared;
        newPageTable[i].isBackedUp = KernelPageTable[i].isBackedUp;
    }

    delete KernelPageTable;
    KernelPageTable = newPageTable;

    for (i = numVirtualPages; i < numRequiredPages; i++)
    {
        newPageTable[i].virtualPage = i;

        DemandAllocation(i);

        newPageTable[i].use = FALSE;
        newPageTable[i].readOnly = FALSE;
        newPageTable[i].shared = TRUE;
        newPageTable[i].isBackedUp = FALSE;

        // No need
        InvertedPageTable[ newPageTable[i].physicalPage ].shared = TRUE;
    }

    int returnValue = numVirtualPages * PageSize;

    numVirtualPages = numRequiredPages;
    RestoreContextOnSwitch();

    return returnValue;
}
/* ------------------------ CUSTOM ------------------------ */

/* ------------------------ CUSTOM ------------------------ */
bool ProcessAddressSpace::DemandAllocation(int vpaddress)
{
    bool flag = FALSE;
    int vpn = vpaddress / PageSize;
    int phyPageNum = GetNextFreePage(-1);

    bzero(&machine->mainMemory[phyPageNum * PageSize], PageSize);

    NoffHeader noffH;
    progExecutable = fileSystem->Open(fileName);
    progExecutable->ReadAt((char *)&noffH, sizeof(noffH), 0);

    if ((noffH.noffMagic != NOFFMAGIC) && (WordToHost(noffH.noffMagic) == NOFFMAGIC))
    {
        SwapHeader(&noffH);
    }
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    if (progExecutable == NULL)
    {
        printf("Unable to open file \n");
        ASSERT(false);
    }
    progExecutable->ReadAt(&(machine->mainMemory[phyPageNum * PageSize]), PageSize, noffH.code.inFileAddr + vpn * PageSize);

    KernelPageTable[vpn].valid = TRUE;
    KernelPageTable[vpn].dirty = FALSE;
    KernelPageTable[vpn].physicalPage = phyPageNum;
    KernelPageTable[vpn].isBackedUp = FALSE;

    InvertedPageTable[phyPageNum].virtualPage = vpn;

    flag = TRUE;
    return flag;
}
/* ------------------------ CUSTOM ------------------------ */

/* ------------------------ CUSTOM ------------------------ */
// Need to set PID of the thread in this function only
unsigned int ProcessAddressSpace::GetNextFreePage(int parentPage)
{
    int pageToBeOccupied = -1;    

    if(numPagesAllocated == NumPhysPages){

        int pageToBeReplaced = -1;

        switch (pageReplaceAlgo)
        {
            case RANDOM:
                pageToBeReplaced = GetRandomPage(parentPage);
            break;
            case FIFO:
                pageToBeReplaced = GetFirstPage(parentPage);
            break;
            case LRU:
                pageToBeReplaced = GetLRUPage(parentPage);
            break;
            case LRUCLOCK:
                pageToBeReplaced = GetLRUCLOCKPage(parentPage);
            break;
        default:
            return numPagesAllocated++;
        }

        ASSERT(pageToBeReplaced != parentPage);
        ASSERT(InvertedPageTable[pageToBeReplaced].shared == FALSE);
        ASSERT(pageToBeReplaced >= 0 && pageToBeReplaced < NumPhysPages);

        if(InvertedPageTable[pageToBeReplaced].dirty == TRUE){
            int vpn = InvertedPageTable[ pageToBeReplaced ].virtualPage;
            ProcessAddressSpace*  tempSpace = threadArray[ InvertedPageTable[ pageToBeReplaced ].threadPID ]->space;
            char* arr = tempSpace->backUpStore;

            for(int i=0 ; i<PageSize ; i++) { arr[ vpn*PageSize + i ] = machine->mainMemory[ pageToBeReplaced*PageSize + i ]; }
            tempSpace->KernelPageTable[vpn].valid = FALSE;
            tempSpace->KernelPageTable[vpn].dirty = FALSE;
            tempSpace->KernelPageTable[vpn].isBackedUp = TRUE;
        }

        pageToBeOccupied = pageToBeReplaced;
    }
    else{
        for(int i=0 ; i<NumPhysPages ; i++){
            if(InvertedPageTable[i].valid == FALSE) { 
                pageToBeOccupied = i;
                numPagesAllocated++;
                break;
            }
        }
        // Testing random
        // if(pageReplaceAlgo == LRU){

        // }
        // else if(pageReplaceAlgo == LRUCLOCK){

        // }
    }

// Note : Virtual page number is set in the callers
    InvertedPageTable[pageToBeOccupied].threadPID = currentThread->GetPID();
    InvertedPageTable[pageToBeOccupied].valid = TRUE;
    InvertedPageTable[pageToBeOccupied].dirty = FALSE;
    InvertedPageTable[pageToBeOccupied].shared = FALSE;

    return pageToBeOccupied;

}
/* ------------------------ CUSTOM ------------------------ */

//---------------------------------------------------------------------------------
//----------------------------------------------------------------------
// ProcessAddressSpace::~ProcessAddressSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

ProcessAddressSpace::~ProcessAddressSpace()
{
    for(int i=0 ; i<numVirtualPages ; i++){
        if(KernelPageTable[i].valid == TRUE){
            // What about shared pages
            InvertedPageTable[ KernelPageTable[i].physicalPage ].valid = FALSE;
        }
    }
    delete backUpStore;
    delete KernelPageTable;
}

//----------------------------------------------------------------------
// ProcessAddressSpace::InitUserModeCPURegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void ProcessAddressSpace::InitUserModeCPURegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
        machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we don't
    // accidentally reference off the end!
    machine->WriteRegister(StackReg, numVirtualPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numVirtualPages * PageSize - 16);
}

//----------------------------------------------------------------------
// ProcessAddressSpace::SaveContextOnSwitch
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void ProcessAddressSpace::SaveContextOnSwitch()
{
}

//----------------------------------------------------------------------
// ProcessAddressSpace::RestoreContextOnSwitch
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void ProcessAddressSpace::RestoreContextOnSwitch()
{
    machine->KernelPageTable = KernelPageTable;
    machine->KernelPageTableSize = numVirtualPages;
}

unsigned
ProcessAddressSpace::GetNumPages()
{
    return numVirtualPages;
}

TranslationEntry *
ProcessAddressSpace::GetPageTable()
{
    return KernelPageTable;
}

/* ------------------------ CUSTOM ------------------------ */
unsigned
ProcessAddressSpace::GetRandomPage(int parentPage){
    int pageToBeReplaced = parentPage;

    while(pageToBeReplaced == parentPage || InvertedPageTable[pageToBeReplaced].shared == TRUE) { 
        pageToBeReplaced = (Random() % NumPhysPages); 
    }

    ASSERT(pageToBeReplaced >= 0 && pageToBeReplaced < NumPhysPages);

    return pageToBeReplaced;
}

unsigned
ProcessAddressSpace::GetFirstPage(int parentPage){
}

unsigned
ProcessAddressSpace::GetLRUPage(int parentPage){
}

unsigned
ProcessAddressSpace::GetLRUCLOCKPage(int parentPage){
}
/* ------------------------ CUSTOM ------------------------ */