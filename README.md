# µPandOS
Implementation of a microkernel operating system on the µRISCV architecture. All the relevant instructions and documentation are in the [Documentation](#Documentation) section.

## Phase 1: The Queues Manager
In this phase, we created two queue managers, one for the processes (PCB) and one for the messages (MSG). 
Each PCB has a `list head p_list`, which is the element that will be used by the kernel to put the process in *a single queue* (e.g. *ready queue* or *waiting msg* queue). Each process also has a parent, siblings, and children. As a `list head` element can only be in a single queue at the same time, only the `list head p_sib` is used in the siblings and children queue: the parent's `p_child` is the same list of the children `p_sib`.
The processes can communicate with each other using a message-passing paradigm and will receive the messages in the `msg_inbox` queue of messages.  

## Phase 2: The Kernel
In this phase we developed the true kernel of the OS: we added the initial booting function, the scheduler, the exception handler, and also a System Service Interface (SSI) process that follows the paradigms of a microkernel and allows other processes to interact with some "kernel" functions while not requesting directly the kernel process.

---
### The Initial Booting
In the [initial](/phase2/initial.c) file we declare and set up all the required structures and global variables, such as the ready queue, the number of existing processes, and so on. We then set up the global interval timer, the SSI as the first process, and the test process that will check that all the functionalities required are well developed. Finally, we call the scheduler.

---
### The Scheduler
Our [scheduler](/phase2/scheduler.c) when called will check some special conditions (if there is a deadlock, if it should wait for an interrupt, and if it has finished all the tests), and then it will set a round robin, and load the next process in the ready queue.  
The policy we use for scheduling is to use a round robin that will be called every 5 milliseconds: it will put the current process at the end of the ready queue and pass control to the first process in the ready queue. Non-blocking syscalls will not call the scheduler again, blocking syscalls will.   

---
### The Exception Handler
When an [exception](/phase2/exceptions.c) is raised, the current process' state will be stored on the BIOSDATAPAGE address and the function `exceptionHandler` will be called. Based on the type of exception, different functions may be called. If the exception is a trap, an illegal syscall, or a TLB exception, the process will be killed (if it does not have a support struct) or the exception will be passed up with a vector to the Support Level (phase 3).  

If a known syscall is called and the process is in kernel (machine) mode it will execute it. There are two possible syscalls handled by the nucleus: `SENDMESSAGE` and `RECVMESSAGE`.
#### Send Message
This function behaves like the asynchronous send of the message-passing paradigm. It may directly unlock a waiting message from the waiting queue if it was waiting for a message or it may put the message in the inbox of the destination process.
#### Receive Message
This function behaves like the asynchronous receive of the message-passing paradigm. If there are already messages in the current process' inbox, it will pop the first, else it will block the process (if it is not already blocked in another queue) and call the scheduler.  

If the cause is an [interrupt](/phase2/interrupts.c), it will be handled based on the type.
#### Interrupts: Interval Timer
When the system-wide timer finishes its clock, it fires an interrupt that we handle with a function that resets the interval timer to the predetermined value and sends a message to the SSI to unlock all the processes waiting for the IT.
#### Interrupts: Local Timer
When the time slice allocated to the current process finishes, an interrupt is fired, at this point, we update the current process' status and put it at the end of the ready queue. Then we call the scheduler.  
#### Interrupts: Device Finished
When a device finishes elaborating the command that it received or encounters a problem, it will fire an interrupt. The function that handles this will check which device has an interrupt active by checking the Interrupting Devices Bit Map, storing the status, sending an ACK to the device, and then sending a message to the SSI to unlock the waiting process and send the result of the operation.

---
### The SSI
The main core of the [SSI](/phase2/ssi.c) will just wait for a message to be received and then call the function that will switch based on the type of SSI request.
#### SSI: Create Process
This function will create a new process with the state and support struct specified in the argument and set it as a child of the sender.
#### SSI: Terminate Process
This function will terminate the process and all its progeny recursively, as in an OS all the processes either suicide or are killed. This function also has to remove the process from all the queues it may be in, as well as update the blocked PCBs count and the total existing PCBs count.
#### SSI: DoIO
The doIO function will help the sender to interface with Input/Output on the devices: it will execute the command as specified and then block the process while it waits for the controller output.
#### SSI: Get CPU Time
This function will send a message to the sender with the time it has used as the active process. The field `p_time` is updated every time a process is stopped, be it because of a `receiveMessage` or because of a Local Timer swap.
#### SSI: Wait For Clock
This function moves the sender's pcb from the queue of processes waiting for a message to the queue of processes waiting for a clock tick (the Interval Timer).
#### SSI: Get Support Data
This function will return a message to the sender with its support structure.
#### SSI: Get Process ID
This function will return a message to the sender with its process ID if the arg is null, else it will send the sender's parent process ID.  

##### Then we have two non-standard functions that we created to unlock processes that are waiting for a message from the ssi to be unlocked from a specific kind of event (interval timer or device IO)
#### SSI: Unblock Process From Device
This function will be called with a message from the exception handler that handles devices' interrupts. It will remove the process from the blocked device list, put it in the waiting MSG list, and then send a message with the device status back to the unlocked process.
#### SSI: Unblock Process From Timer
This function will be called with a message from the exception handler that handles the Interval Timer. It will remove all the processes from the waiting Interval Timer list, put them in the waiting MSG list, and then send a message to unlock them.

---
### Additional Choices And Details
#### Edited the `pcb_t` structure
We have added a new field to the original structure of `pcb_t`, as we deemed it necessary to have an additional `blocked` field to check if the process was already in a queue of blocked pcbs or not. This is necessary because each process can be waiting for both a message and the Interval Timer or a Device IO. As each process `p_list` cannot be in two queues at the same time, we manage the insertion in each queue with this field.  
  
Let's take as a case study the doIO operation. The process sends a request to the SSI and then does a syscall to receive the SSI's answer. If the local timer sends an interrupt before the process calls the receive, the control will eventually pass to the ssi, whose `doIO` will insert the process' pcb in the list for the PCBs waiting for the device and it will set `blocked` to 1. The control will eventually then come back to the original process that will call receive a message: at this point, the process is already blocked, so it will not be put on the waiting msg list. If the local timer does not fire before the blocking syscall, then the process will be put in the waiting msg list and `blocked` will be set to 1. Then the scheduler will be called and the SSI's `doIO` function will remove the process from the waiting msg list and it will put it in the list waiting for the device.  
When the device sends the interrupt and it is received, the process waiting for the interrupt will be removed from the waiting device queue and put in the waiting message queue. The interrupt handler will send a message to the SSI, which will in turn send a message to the process and effectively unlock it.  
In this way, the PCB was never in two different queues at the same time, just as we needed it.

#### Sending messages to the SSI while in an exception
When some exceptions are raised we need to tell the SSI to send messages to unlock PCBs waiting for the Interval Timer of a device's IO. As we are already in an exception we cannot use the standard function `SYSCALL` to send messages, as it would override the current content of `BIOSDATAPAGE` and thus lose the current process's status. So we manually call the `sendMessage` function with a fake status to send a message to the SSI.

#### Aliasing the SSI pcb
For security reasons, we cannot expose the SSI pcb (`true_ssi_pcb`) as a global variable, so we create an alias (`ssi_pcb`) with a fixed value to an address that we know is not available (in our case `42`, which is in kseg0). In the `receiveMessage` and `sendMessage` functions we handle the swap between the *true* address and the alias.

#### `p_time` updating policy
Each `pcb_t` has a field called `p_time` that keeps track of how much time the process was active (i.e. used the CPU). We decided that it was sufficient to update this field only when the process was blocked by an event that may be either the Local Timer (`p_time += TIMESLICE`) and by the blocking Syscall (update the field with the time passed: `p_time += TIMESLICE - getTIMER()`). When a process asks the SSI how much time it has used the CPU, it will do a blocking syscall or it may be interrupted by the Local Timer, so on all the relevant occasions the field is correctly updated.

#### setMIE before WAIT
When the scheduler needs to wait for the next interrupt, we have to ignore the local timer or the waiting scheduler will be continuously interrupted while it may be waiting for the interval timer (that is, in our case, 20 times the local timer). To avoid this we set the MIE to listen all but the Local Timer interrupts.

#### Memcpy
We had to reimplement memcpy in the [utils](/phase2/utils.c) as the compiler substituted the * (dereferencing) with memcpy automatically when handling structures.

---
### Theoretically Possible Problems
Here are two problems that are theoretically possible but practically are never encountered in the test.

#### Not Checking Manual SendMessage
When the interrupts handler sends a message to the SSI because the Clock has ticked or a device has finished we do not check if the message was actually sent or not (it may not be sent if there are no more messages left to allocate). This event is almost impossible and can only be caused by a malicious process that fills up the message table by sending messages to a process that does not call receive, or by never waiting for an answer, as almost all the sent messages will receive an answer.

#### Multiple Requests To The Same Controller.
As of now, no checks are performed when a process requires a DoIO operation. Subsequent commands to a device will override previous ones if they haven't already been performed. This could be handled by creating a queue for commands and not only processes as it is now. This problem doesn't appear in the tests since only one process accesses the terminal. All other processes send the strings they desire to print to this special process, which forwards them to the device via the SSI.

## Phase 3: The Support Level

## Documentation
- [𝜇PandOS: Setup Tutorial](/docs/TUTORIAL.md)
- [𝜇PandOS: Phase 1](/docs/MicroPandOSPhase1Spec.pdf)
- [𝜇PandOS: Phase 2](/docs/MicroPandOSPhase2Spec.pdf)
- [𝜇PandOS: Phase 3](/docs/MicroPandOSPhase3Spec.pdf)
- [Porting of the 𝜇MPS3 Educational Emulator to RISC-V](https://amslaurea.unibo.it/29151/1/GianmariaRovelli_BozzaTesi_09_07_2023.pdf)
- [µMPS3 Principles of Operation](https://www.cs.unibo.it/~renzo/doc/umps3/uMPS3princOfOperations.pdf)
- [The RISC-V Instruction Set Manual, Volume I: Unprivileged ISA](https://github.com/riscv/riscv-isa-manual/releases/tag/Ratified-IMAFDQC)
- [The RICV-V Instruction Set Manual, Volume II: Privileged Architecture](https://github.com/riscv/riscv-isa-manual/releases/tag/Priv-v1.12)
