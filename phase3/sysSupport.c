#include "headers/sysSupport.h"
#include "../headers/const.h"
#include "headers/initProc.h"
#include <uriscv/liburiscv.h>

void SYSCALLExceptionHandler(support_t *support_struct);
void programTrapExceptionHandler();

// support function used to get a proc's support struct in phase 3
support_t *getSupStruct() {
  support_t *sup_struct;
  ssi_payload_t getsup_payload = {
      .service_code = GETSUPPORTPTR,
      .arg = NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&getsup_payload),
          0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&sup_struct),
          0);

  return sup_struct;
}


// entry point of the general exception handler
void generalExceptionHandler() {

  // get support struct
  support_t *support_struct = getSupStruct();

  // get process state
  state_t *exceptionState = &(support_struct->sup_exceptState[GENERALEXCEPT]);

  // get the exception cause
  int cause = exceptionState->cause;

  if (cause == SYSEXCEPTION) {

    // call the syscall exception handler
    SYSCALLExceptionHandler(support_struct);

    // load back the process state after the exception is handled
    LDST(exceptionState);

  } else {
    // call the trap exception handler
    programTrapExceptionHandler();
  }
}

// handles syscall on user level
void SYSCALLExceptionHandler(support_t *support_struct) {
  state_t *proc_state = &(support_struct->sup_exceptState[GENERALEXCEPT]);

  if (proc_state->reg_a0 == SENDMSG) {

    // get the destination process
    pcb_t *destination = (pcb_t *)proc_state->reg_a1;

    if (destination == PARENT) 
      destination = current_process->p_parent;

    // syscall to send the message (reg_a2 is the payload)
    SYSCALL(SENDMESSAGE, (unsigned int)destination, (unsigned int)proc_state->reg_a2, 0);

  } else if (proc_state->reg_a0 == RECEIVEMSG) {

    // get the sender process
    pcb_t *sender = (pcb_t *)proc_state->reg_a1;

    if (sender == PARENT)
      sender = current_process->p_parent;

    // syscall to receive the message (reg_a2 is the payload)
    SYSCALL(RECEIVEMESSAGE, (unsigned int)sender, (unsigned int)proc_state->reg_a2, 0);
  }

  // set the result of the syscall
  proc_state->reg_a0 = current_process->p_s.reg_a0;

  // increment the pc
  proc_state->pc_epc += 4;
}

// all the traps not handled cause a suicide
void programTrapExceptionHandler() {
  // send the release mutual exclusion if held message
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_pcb, 1, 0);

  ssi_payload_t sst_payload = {
  .service_code = TERMINATE,
  .arg = 0,
  };
  // send the terminate message to the parent (the sst)
  SYSCALL(SENDMESSAGE, (unsigned int)current_process->p_parent, (unsigned int)&sst_payload, 0);

  // block the process
  SYSCALL(RECEIVEMESSAGE, (unsigned int)current_process->p_parent, 0, 0);
}