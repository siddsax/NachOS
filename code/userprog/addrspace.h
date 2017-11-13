#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define UserStackSize 1024

class ProcessAddressSpace
{
public:
  ProcessAddressSpace(OpenFile *executable);

  ProcessAddressSpace(ProcessAddressSpace *parentSpace);

  ~ProcessAddressSpace();

  void InitUserModeCPURegisters();

  void SaveContextOnSwitch();
  void RestoreContextOnSwitch();

  unsigned GetNumPages();

  TranslationEntry *GetPageTable();

  /* ------------------------ CUSTOM ------------------------ */
  unsigned int AllocateSharedMemory(int size);

  unsigned int GetNextFreePage(int parentPage);
  bool DemandAllocation(int vpaddress);

  char *fileName;
  /* ------------------------ CUSTOM ------------------------ */

private:
  TranslationEntry *KernelPageTable;

  unsigned int numVirtualPages;

  OpenFile *progExecutable;
};

#endif
