#include "headers/sysSupport.h"
#include "../headers/const.h"
#include "headers/initProc.h"
#include <uriscv/liburiscv.h>

void SYSCALLExceptionHandler(support_t *support_struct);
void programTrapExceptionHandler();

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
    LDST(&exceptionState);

  } else {
    // call the trap exception handler
    programTrapExceptionHandler();
  }
}

void SYSCALLExceptionHandler(support_t *support_struct) {
  state_t *proc_state = &(support_struct->sup_exceptState[GENERALEXCEPT]);

  if (proc_state->reg_a0 == SENDMSG) {

    // get the destination process
    pcb_t *destination = (pcb_t *)proc_state->reg_a1;

    if (destination == PARENT) {
      // send the message to the parent
      destination = current_process->p_parent;
    }

    // get the payload
    unsigned int payload = proc_state->reg_a2;

    // syscall to send the message
    SYSCALL(SENDMESSAGE, (unsigned int)destination, payload, 0);

  } else if (proc_state->reg_a0 == RECEIVEMSG) {

    // get the sender process
    pcb_t *sender = (pcb_t *)proc_state->reg_a1;

    unsigned int *payload = (unsigned int *)current_process->p_s.reg_a2;

    // syscall to receive the message
    SYSCALL(RECEIVEMESSAGE, (unsigned int)sender, (unsigned int)payload, 0);
  }

  // set the result of the syscall
  proc_state->reg_a0 = current_process->p_s.reg_a0;

  // increment the pc
  proc_state->pc_epc += 4;
}

void programTrapExceptionHandler() {
  // send the release mutual exclusin message
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_pcb, 1, 0);

  // send the terminate message to the parent
  SYSCALL(SENDMESSAGE, (unsigned int)current_process->p_parent, TERMINATE, 0);

  // BLock the process
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ANYMESSAGE, 0, 0);
}
