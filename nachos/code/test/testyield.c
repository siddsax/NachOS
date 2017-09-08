#include "syscall.h"

int
main()
{
    int x, i;
    
    int t = 10;

    x = syscall_wrapper_Fork();
    //syscall_wrapper_Fork();
    if (x == 0) {
        syscall_wrapper_PrintString("*** thread ");
    }/*
    for (i=0; i<5; i++) {
       syscall_wrapper_PrintString("*** thread ");
       syscall_wrapper_PrintInt(syscall_wrapper_GetPID());
       syscall_wrapper_PrintString(" looped ");
       syscall_wrapper_PrintInt(i);
       syscall_wrapper_PrintString(" times.\n");
       syscall_wrapper_Yield();
    }
    if (x != 0) {
       syscall_wrapper_PrintString("Before join.\n");
       //syscall_wrapper_Join(x);
       syscall_wrapper_PrintString("After join.\n");
    }
    syscall_wrapper_PrintString("Fork Done");
    syscall_wrapper_PrintInt(t);*/
    return 0;
}
