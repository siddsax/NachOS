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

ProcessAddressSpace::ProcessAddressSpace(OpenFile *executable, char *filename)
{
    fileName = filename;
    progExecutable = executable;

    NoffHeader noffH;
    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;

    progExecutable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && (WordToHost(noffH.noffMagic) == NOFFMAGIC))
    {
        SwapHeader(&noffH);
    }
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;

    numVirtualPages = divRoundUp(size, PageSize);
    size = numVirtualPages * PageSize;

    /* ------------------------ CUSTOM ------------------------ */
    backup = new char[size];

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
          numVirtualPages, size);

    KernelPageTable = new TranslationEntry[numVirtualPages];
    if (pageReplaceAlgo == 0)
    {
        ASSERT(numVirtualPages + numPagesAllocated <= NumPhysPages);

        bzero(&machine->mainMemory[numPagesAllocated * PageSize], size);

        for (i = 0; i < numVirtualPages; i++)
        {
            KernelPageTable[i].virtualPage = i;
            KernelPageTable[i].physicalPage = GetPhysicalPage(-1, i);
            KernelPageTable[i].valid = TRUE;
            KernelPageTable[i].use = FALSE;
            KernelPageTable[i].dirty = FALSE;
            KernelPageTable[i].readOnly = FALSE;
            KernelPageTable[i].shared = FALSE;
            KernelPageTable[i].backed = FALSE;
        }

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
            KernelPageTable[i].shared = FALSE;
            KernelPageTable[i].backed = FALSE;
        }
    }
    /* ------------------------ CUSTOM ------------------------ */
}

ProcessAddressSpace::ProcessAddressSpace(ProcessAddressSpace *parentSpace)
{
    /* ------------------------ CUSTOM ------------------------ */
    fileName = parentSpace->fileName;
    progExecutable = fileSystem->Open(fileName);
    if (progExecutable == NULL)
    {
        printf("Unable to open file %s\n", fileName);
        ASSERT(false);
    }

    numVirtualPages = parentSpace->GetNumPages();
    unsigned i, size = numVirtualPages * PageSize;

    KernelPageTable = new TranslationEntry[numVirtualPages];
    backup = new char[size];
    /* ------------------------ CUSTOM ------------------------ */
}

void ProcessAddressSpace::InitiateForkedProcessAddressSpace(ProcessAddressSpace *parentSpace, int childPID)
{
    unsigned i, j, size = numVirtualPages * PageSize;

    /* ------------------------ CUSTOM ------------------------ */
    fileName = parentSpace->fileName;
    progExecutable = fileSystem->Open(fileName);
    if (progExecutable == NULL)
    {
        printf("Unable to open file %s\n", fileName);
        ASSERT(false);
    }
    numVirtualPages = parentSpace->GetNumPages();
    /* ------------------------ CUSTOM ------------------------ */

    if (pageReplaceAlgo == 0)
    {
        ASSERT(numVirtualPages + numPagesAllocated <= NumPhysPages);
    }

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
          numVirtualPages, size);

    TranslationEntry *parentPageTable = parentSpace->GetPageTable();
    KernelPageTable = new TranslationEntry[numVirtualPages];

    unsigned startAddrParent, startAddrChild;

    /* ------------------------ CUSTOM ------------------------ */
    backup = new char[size];
    for (i = 0; i < size; i++)
    {
        backup[i] = parentSpace->backup[i];
    }

    for (int i = 0; i < numVirtualPages; i++)
    {
        KernelPageTable[i].virtualPage = i;
        KernelPageTable[i].use = parentPageTable[i].use;
        KernelPageTable[i].readOnly = parentPageTable[i].readOnly;
        KernelPageTable[i].dirty = parentPageTable[i].dirty;
        KernelPageTable[i].shared = parentPageTable[i].shared;
        KernelPageTable[i].backed = parentPageTable[i].backed;
        KernelPageTable[i].valid = parentPageTable[i].valid;

        if (parentPageTable[i].valid == TRUE && parentPageTable[i].shared == FALSE)
        {
            KernelPageTable[i].physicalPage = GetPhysicalPage(parentPageTable[i].physicalPage, i);
            physicalPagesList[KernelPageTable[i].physicalPage].threadPID = childPID;

            startAddrParent = parentPageTable[i].physicalPage * PageSize;
            startAddrChild = KernelPageTable[i].physicalPage * PageSize;

            for (int j = 0; j < PageSize; j++)
            {
                machine->mainMemory[startAddrChild + j] = machine->mainMemory[startAddrParent + j];
            }

            // currentThread->SortedInsertInWaitQueue(1000 + stats->totalTicks);
        }
        else
        {
            KernelPageTable[i].physicalPage = parentPageTable[i].physicalPage;
        }
    }
    /* ------------------------ CUSTOM ------------------------ */
}

/* ------------------------ CUSTOM ------------------------ */
unsigned int ProcessAddressSpace::AllocateSharedMemory(int size)
{
    unsigned int numRequiredPages = numVirtualPages + divRoundUp(size, PageSize);
    numSharedPages += divRoundUp(size, PageSize);

    ASSERT(numSharedPages < NumPhysPages);

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
        newPageTable[i].backed = KernelPageTable[i].backed;
    }

    char *newBackup;
    newBackup = new char[numRequiredPages * PageSize];

    for (i = 0; i < numVirtualPages * PageSize; i++)
    {
        newBackup[i] = backup[i];
    }

    for (i = numVirtualPages; i < numRequiredPages; i++)
    {
        newPageTable[i].virtualPage = i;
        newPageTable[i].physicalPage = GetPhysicalPage(-1, i);
        newPageTable[i].valid = TRUE;
        newPageTable[i].dirty = FALSE;
        newPageTable[i].use = FALSE;
        newPageTable[i].readOnly = FALSE;
        newPageTable[i].shared = TRUE;
        newPageTable[i].backed = FALSE;

        /* ------------------------ CUSTOM ------------------------ */
        physicalPagesList[newPageTable[i].physicalPage].shared = TRUE;
        /* ------------------------ CUSTOM ------------------------ */
    }

    delete KernelPageTable;
    KernelPageTable = newPageTable;

    delete backup;
    backup = newBackup;

    int returnValue = numVirtualPages * PageSize;

    numVirtualPages = numRequiredPages;
    RestoreContextOnSwitch();

    return returnValue;
}
/* ------------------------ CUSTOM ------------------------ */

/* ------------------------ CUSTOM ------------------------ */
void ProcessAddressSpace::DemandAllocation(int vpaddress)
{
    bool flag = FALSE;
    int vpn = vpaddress / PageSize;
    int phyPageNum = GetPhysicalPage(-1, vpn);

    bzero(&machine->mainMemory[phyPageNum * PageSize], PageSize);

    if (KernelPageTable[vpn].backed == TRUE)
    {
        unsigned int i;
        for (i = 0; i < PageSize; i++)
        {
            machine->mainMemory[phyPageNum * PageSize + i] = backup[vpn * PageSize + i];
        }
    }
    else
    {
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
    }

    KernelPageTable[vpn].valid = TRUE;
    KernelPageTable[vpn].dirty = FALSE;
    KernelPageTable[vpn].physicalPage = phyPageNum;
}
/* ------------------------ CUSTOM ------------------------ */

/* ------------------------ CUSTOM ------------------------ */
unsigned int ProcessAddressSpace::GetPhysicalPage(int parentPagePhysicalNumber, int virtualPage)
{

    if (pageReplaceAlgo == 0)
    {
        return numPagesAllocated++;
    }

    int i, pageToBeOccupied = -1;

    if (numPagesAllocated < NumPhysPages)
    {
        for (i = 0; i < NumPhysPages; i++)
        {
            if (physicalPagesList[i].threadPID == -1)
            {
                pageToBeOccupied = i;
                numPagesAllocated++;
                break;
            }
        }
    }
    else
    {
        int pageToBeReplaced = -1;

        switch (pageReplaceAlgo)
        {
        case RANDOM:
            pageToBeReplaced = GetRandomPage(parentPagePhysicalNumber);
            break;
        case FIFO:
            pageToBeReplaced = GetFirstPage(parentPagePhysicalNumber);
            break;
        // case LRU:
        //     pageToBeReplaced = GetLRUPage(parentPagePhysicalNumber);
        //     break;
        case LRUCLOCK:
            pageToBeReplaced = GetLRUCLOCKPage(parentPagePhysicalNumber);
            break;
        default:
            ASSERT(FALSE);
        }

        ASSERT(pageToBeReplaced != parentPagePhysicalNumber);
        ASSERT(physicalPagesList[pageToBeReplaced].virtualPage != -1);
        ASSERT(physicalPagesList[pageToBeReplaced].shared == FALSE);
        ASSERT(pageToBeReplaced >= 0 && pageToBeReplaced < NumPhysPages);

        int vpn = physicalPagesList[pageToBeReplaced].virtualPage;
        ProcessAddressSpace *threadAddressSpace = threadArray[physicalPagesList[pageToBeReplaced].threadPID]->space;

        if (threadAddressSpace->KernelPageTable[vpn].dirty == TRUE)
        {
            char *threadBackup = threadAddressSpace->backup;

            for (i = 0; i < PageSize; i++)
            {
                threadBackup[vpn * PageSize + i] = machine->mainMemory[pageToBeReplaced * PageSize + i];
            }

            threadAddressSpace->KernelPageTable[vpn].backed = TRUE;
        }

        pageToBeOccupied = pageToBeReplaced;

        threadAddressSpace->KernelPageTable[vpn].valid = FALSE;
        threadAddressSpace->KernelPageTable[vpn].dirty = FALSE;
    }

    bzero(&machine->mainMemory[pageToBeOccupied * PageSize], PageSize);

    physicalPagesList[pageToBeOccupied].threadPID = currentThread->GetPID();
    physicalPagesList[pageToBeOccupied].virtualPage = virtualPage;
    physicalPagesList[pageToBeOccupied].shared = FALSE;

    /* ------------------------ CUSTOM ------------------------ */
    if(pageReplaceAlgo == 2){
        FIFOArray[pageToBeOccupied] = stats->totalTicks;
    }
    /* ------------------------ CUSTOM ------------------------ */

    /* ------------------------ CUSTOM ------------------------ */
    if(pageReplaceAlgo == 4){
        referenceArray[pageToBeOccupied] = TRUE;
    }
    /* ------------------------ CUSTOM ------------------------ */

    return pageToBeOccupied;
}
/* ------------------------ CUSTOM ------------------------ */

/* ------------------------ CUSTOM ------------------------ */
unsigned int ProcessAddressSpace::GetRandomPage(int parentPagePhysicalNumber)
{
    int pageToBeReplaced = parentPagePhysicalNumber;

    while (pageToBeReplaced == parentPagePhysicalNumber || physicalPagesList[pageToBeReplaced].shared == TRUE)
    {
        pageToBeReplaced = Random() % NumPhysPages;
    }

    return pageToBeReplaced;
}

unsigned int ProcessAddressSpace::GetFirstPage(int parentPagePhysicalNumber)
{
    int pageToBeReplaced = -1, minTicks = INF;

    for(int i=0 ; i<NumPhysPages ; i++)
    {
        if(i == parentPagePhysicalNumber || physicalPagesList[i].shared == TRUE || FIFOArray[i] == INF) { continue; }

        if(FIFOArray[i] < minTicks)
        {
            minTicks = FIFOArray[i];
            pageToBeReplaced = i;
        }
    }

    return pageToBeReplaced;
}

unsigned int ProcessAddressSpace::GetLRUPage(int parentPagePhysicalNumber)
{
    return -1;
}

unsigned int ProcessAddressSpace::GetLRUCLOCKPage(int parentPagePhysicalNumber)
{
    int pageToBeReplaced = -1;

    while(pageToBeReplaced == -1)
    {
        if(pointReference == parentPagePhysicalNumber || physicalPagesList[pointReference].shared == TRUE) { 
            pointReference = (pointReference + 1) % NumPhysPages;
            continue; 
        }

        if(referenceArray[pointReference] == TRUE)
        { 
            referenceArray[pointReference] = FALSE; 
        }
        else
        {
            pageToBeReplaced = pointReference;
            break;
        }

        pointReference = (pointReference + 1) % NumPhysPages;
    }

    return pageToBeReplaced;
}
/* ------------------------ CUSTOM ------------------------ */

ProcessAddressSpace::~ProcessAddressSpace()
{
    int i, phyPage;
    for (i = 0; i < numVirtualPages; i++)
    {
        if (KernelPageTable[i].valid)
        {
            phyPage = KernelPageTable[i].physicalPage;
            if (!KernelPageTable[i].shared)
            {
                physicalPagesList[phyPage].virtualPage = -1;
                numPagesAllocated--;
                /* ------------------------ CUSTOM ------------------------ */
                if(pageReplaceAlgo == 2){
                    FIFOArray[phyPage] = INF;
                }
                /* ------------------------ CUSTOM ------------------------ */
            }

            physicalPagesList[phyPage].threadPID = -1;
        }
    }

    delete KernelPageTable;
    delete backup;
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
