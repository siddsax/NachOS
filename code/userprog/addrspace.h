#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define UserStackSize 1024

class ProcessAddressSpace
{
public:
  /* ------------------------ CUSTOM ------------------------ */
  ProcessAddressSpace(OpenFile *executable, char *filename);
  ProcessAddressSpace(ProcessAddressSpace *parentSpace, int childPID);
  /* ------------------------ CUSTOM ------------------------ */

  ~ProcessAddressSpace();

  void InitUserModeCPURegisters();

  void SaveContextOnSwitch();
  void RestoreContextOnSwitch();

  unsigned GetNumPages();

  TranslationEntry *GetPageTable();

  /* ------------------------ CUSTOM ------------------------ */
  unsigned int AllocateSharedMemory(int size);

  unsigned int GetPhysicalPage(int parentPage, int virtualPage);
  void DemandAllocation(int vpaddress);

  char *fileName;

  char *backup;

  unsigned int GetRandomPage(int parentPagePhysicalNumber);
  /* ------------------------ CUSTOM ------------------------ */

private:
  TranslationEntry *KernelPageTable;

  unsigned int numVirtualPages;

  OpenFile *progExecutable;
};

#endif
