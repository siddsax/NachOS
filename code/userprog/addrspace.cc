#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

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
        }
    }
    /* ------------------------ CUSTOM ------------------------ */
}

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

        if (parentPageTable[i].valid == TRUE && parentPageTable[i].shared == FALSE)
        {
            KernelPageTable[i].physicalPage = GetNextFreePage(parentPageTable[i].physicalPage);
            KernelPageTable[i].valid = TRUE;

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

    flag = TRUE;
    return flag;
}
/* ------------------------ CUSTOM ------------------------ */

/* ------------------------ CUSTOM ------------------------ */
unsigned int ProcessAddressSpace::GetNextFreePage(int currentPage)
{
    switch (pageReplaceAlgo)
    {
    default:
        return numPagesAllocated++;
    }
}
/* ------------------------ CUSTOM ------------------------ */

ProcessAddressSpace::~ProcessAddressSpace()
{
    delete KernelPageTable;
}

void ProcessAddressSpace::InitUserModeCPURegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
        machine->WriteRegister(i, 0);

    machine->WriteRegister(PCReg, 0);

    machine->WriteRegister(NextPCReg, 4);

    machine->WriteRegister(StackReg, numVirtualPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numVirtualPages * PageSize - 16);
}

void ProcessAddressSpace::SaveContextOnSwitch()
{
}

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
