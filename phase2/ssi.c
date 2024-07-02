#include "./headers/ssi.h"
#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include "./headers/utils.h"
#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>

void SSIRequest(pcb_t *sender, int service, void *arg);

/**
 * This function is the main function of the SSI process that will forever wait
 * for a message, get the payload and then pass id to the SSIRequest handler
 */
void SSI_function_entry_point() {
  while (1) {
    ssi_payload_t *payload;
    pcb_t *sender =
        (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);
    SSIRequest(sender, payload->service_code, payload->arg);
  }
}

/**
 * This function will create a new process with the state and support struct
 * specified in the arg and set it as child of the sender
 */
unsigned int createProcess(pcb_t *sender, ssi_create_process_t *arg) {
  pcb_t *new_pcb = allocPcb();
  if (new_pcb == NULL) // no new proc allocable
    return NOPROC;

  // initialize the new pcb's fields to the values specified by arg
  new_pcb->p_s = *arg->state;
  new_pcb->p_supportStruct = arg->support;

  // insert the new process as a child of the sender and then in the ready queue
  insertChild(sender, new_pcb);
  insertProcQ(&ready_queue, new_pcb);
  process_count++;

  // return pointer to the pcb of the created process
  return (unsigned int)new_pcb;
}

/**
 * This function will terminate the process and all its progeny recursively
 */
void terminateProcess(pcb_t *proc) {
  // if the process has any children, kill them with the same function
  while (!emptyProcQ(&proc->p_child)) {
    pcb_t *removedChild = container_of(proc->p_child.next, pcb_t, p_sib);
    terminateProcess(removedChild);
  }

  // decrease the process count and remove the specified process: if it is not
  // the current process or in the ready queue or in the message queue then it's
  // in a blocking queue. In this case decrease the soft block count as well
  process_count--;
  if (current_process == proc)
    current_process = NULL;
  else if (outProcQ(&ready_queue, proc) == NULL &&
           outProcQ(&waiting_MSG, proc) == NULL) {
    soft_block_count--;
    // delete it from whichever queue it's in
    list_del(&proc->p_list);
  }
  // detatch from parent and siblings
  outChild(proc);
  // free space for a new process
  freePcb(proc);
}

/**
 * This helper function will remove the process from the waiting_MSG queue
 */
static inline void removeFromMessageQueue(pcb_t *process) {
  outProcQ(&waiting_MSG, process);
  if (outProcQ(&ready_queue, process) != NULL)
    process->blocked = 2;
  else
    process->blocked = 1;
}

/**
 * The doIO function will help the sender to interface with Input/Output on the
 * devices: it will execute the command as specified and then block the process
 * while it waits for the controller output
 */
void doIO(pcb_t *sender, ssi_do_io_t *do_io) {
  // remove the process from the waiting message queue: a pcb can stay in only
  // one queue at a time
  removeFromMessageQueue(sender);
  soft_block_count++;

  // block the pcb: get the device and controller from the address
  unsigned short device_number =
      ((unsigned int)do_io->commandAddr - START_DEVREG) /
      (DEVPERINT * DEVREGSIZE);
  unsigned short controller_number =
      (((unsigned int)do_io->commandAddr - START_DEVREG) / DEVREGSIZE) %
      DEVPERINT;

  // block the process and insert its pcb in the appropriate waiting list
  insertProcQ(&blocked_pcbs[device_number][controller_number], sender);

  // write command value on the device address specified as the command address
  *(do_io->commandAddr) = do_io->commandValue;
}

/**
 * This function will be called with a message from the exception handler that
 * handles devices' interrupts. It will remove the process from the blocked
 * device list, put it in the waiting_MSG list and then send a message with the
 * device status back to the unlocked process
 */
static inline void unblockProcessFromDevice(ssi_unblock_do_io_t *do_io) {
  pcb_t *proc = removeProcQ(&blocked_pcbs[do_io->device][do_io->controller]);
  if (proc->blocked == 2) {
    proc->blocked = 0;
    insertProcQ(&ready_queue, proc);
  } else
    insertProcQ(&waiting_MSG, proc);
  soft_block_count--;
  SYSCALL(SENDMESSAGE, (unsigned int)proc, (unsigned int)do_io->status, 0);
}

/**
 * This function will be called with a message from the exception handler that
 * handles the Interval Timer. It will remove all the processes from the
 * waiting_IT list, put it in the waiting_MSG list and then send a message to
 * unlock them
 */
static inline void unblockProcessFromTimer() {
  // while there are still processes in the waiting queue, unlock them
  while (!emptyProcQ(&waiting_IT)) {
    pcb_t *process = removeProcQ(&waiting_IT);
    if (process->blocked == 2) {
      process->blocked = 0;
      insertProcQ(&ready_queue, process);
    } else
      insertProcQ(&waiting_MSG, process);
    soft_block_count--;
    SYSCALL(SENDMESSAGE, (unsigned int)process, 0, 0);
  }
}

/**
 * This function will send a message to the sender with the time it has used as
 * the active process. The field p_time is updated every time a process is
 * stopped, be it because of a receiveMessage or because of a PLT swap
 */
static inline void getCPUTime(pcb_t *sender) {
  SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)sender->p_time, 0);
}

/**
 * This function moves the sender's pcb from the queue of processes waiting for
 * a message to the queue of processes waiting for a clock tick.
 */
static inline void waitForClock(pcb_t *sender) {
  removeFromMessageQueue(sender);
  soft_block_count++;
  insertProcQ(&waiting_IT, sender);
}

/**
 * This function will return a message to the sender with its own support
 * struct.
 */
static inline void getSupportData(pcb_t *sender) {
  SYSCALL(SENDMESSAGE, (unsigned int)sender,
          (unsigned int)sender->p_supportStruct, 0);
}

/**
 * This function will return a message to the sender with its own process id if
 * the arg is null, else it will send the sender's parent process id
 */
static void getProcessID(pcb_t *sender, void *arg) {
  if (arg == NULL)
    SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)sender->p_pid, 0);
  else if (sender->p_parent != NULL)
    SYSCALL(SENDMESSAGE, (unsigned int)sender,
            (unsigned int)sender->p_parent->p_pid, 0);
  else
    SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
}

/**
 * This function will switch between the services based on the service code and
 * call the corresponding function. If the code is invalid it will kill the
 * process that requested the invalid service.
 * @param sender the process that sent the message to the SSI and requested the
 * service
 * @param service the service requested to the SSI
 * @param arg the arg that is passed in the message as a parameter for the
 * service
 */
void SSIRequest(pcb_t *sender, int service, void *arg) {
  switch (service) {
  case CREATEPROCESS:
    pcb_t *new_proc =
        (pcb_t *)createProcess(sender, (ssi_create_process_t *)arg);
    SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)new_proc, 0);
    break;
  case TERMPROCESS:
    if (arg == NULL)
      arg = sender;
    terminateProcess(arg);
    if (arg != sender)
      SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
    break;
  case DOIO:
    doIO(sender, (ssi_do_io_t *)arg);
    break;
  case GETTIME:
    getCPUTime(sender);
    break;
  case CLOCKWAIT:
    waitForClock(sender);
    break;
  case GETSUPPORTPTR:
    getSupportData(sender);
    break;
  case GETPROCESSID:
    getProcessID(sender, arg);
    break;
  case UNBLOCKPROCESSDEVICE:
    unblockProcessFromDevice((ssi_unblock_do_io_t *)arg);
    break;
  case UNBLOCKPROCESSTIMER:
    unblockProcessFromTimer();
    break;
  default: // terminate process and all of its children
    terminateProcess(sender);
  }
}
