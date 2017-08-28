# NachOS Implementation

System calls to be implemented -

* GetReg
* GetPA
* GetPIP
* GetPPID
* NumInstr
* Time
* Yield
* Sleep
* Exec
* Exit
* Join
* Fork

## General Workflow

The system calls are defined within system call wrapper 
functions which are called from within a user program.
These system calls are responsible for incrementing
the program counter and hence handle the program flow.
The system calls are hard coded in the ExceptionHandler()
function in exception.cc file in an if...else block.

The type of system call / exception is passed as an 
argument *which* in this function. The values passed
to and from these calls are handled through machine
registers. *Register 2* is used to pass return value
and *Registers 4 to 7* are used to pass arguments.

Common steps in most of the functions is reading the
arguments from machine registers, writing the return
value to the machine registers and incrementing the
program counter.

**Reading Arguments**

    machine->readRegister(4) returns the value read
    in the 4th register. Here machine is an object
    instance of the complete NachOS machine.

**Returning Value**

    machine->writeRegister(2, value) writes the *value*
    in the corresponding register, i.e. register 2.

**Incrementing Program Counter**

    machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
    machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
    machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);

    Since Program Counter is also a register, we can
    just increment the register value by 4 to update
    the program counter. This step in crucial to
    continue execution of the user program, otherwise
    the current system call will keep on executing in
    an infinite loop.




##Implementation of Each System Call

Below is the individual implementation of each individual system call -

###GetPID

###GetPPID

###Time

###NumInstr

###Sleep


