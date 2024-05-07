#include "headers/sysSupport.h"
#include "../headers/const.h"
#include <uriscv/liburiscv.h>

void SYSCALLExceptionHandler(state_t *proc_state);
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
  int cause = (exceptionState->cause & GETEXECCODE) >> CAUSESHIFT;

  if (cause == SYSEXCEPTION) {
    // call the syscall exception handler
    SYSCALLExceptionHandler(exceptionState);
  } else {
    // call the trap exception handler
    programTrapExceptionHandler();
  }

  // load back the process state after the exception is handled
  LDST(&exceptionState);
}

void SYSCALLExceptionHandler(state_t *proc_state) {
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

  // increment the pc
  current_process->p_s.pc_epc += 4;
}

void programTrapExceptionHandler() {
  ssi_payload_t payload;
  payload.service_code = TERMPROCESS;
  payload.arg = NULL;
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);

  // TODO: se sta aspettando la mutua esclusione bisogna rilasciarla
}
