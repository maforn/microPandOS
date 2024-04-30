#include "./headers/initProc.h"
#include "../phase2/headers/initial.h"
#include <uriscv/liburiscv.h>

pcb_PTR initiator_pcb, swap_mutex_pcb, sst_pcbs[UPROCMAX];

extern pcb_t *ssi_pcb;

state_t sst_state[UPROCMAX], mutex_state;
// TODO: check if more camps are required such as sup_stackTLB
static support_t uproc_sup[UPROCMAX];

pcb_t *create_process(state_t *s, support_t *sup) {
  pcb_t *p;
  ssi_create_process_t ssi_create_process = {
      .state = s,
      .support = sup,
  };
  ssi_payload_t payload = {
      .service_code = CREATEPROCESS,
      .arg = &ssi_create_process,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&p), 0);
  return p;
}

void swap_mutex() {
  pcb_t *p;
  while (1) {
    // wait for someone to request the mutual exlusion
    SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)(&p), 0);
    // give the ok to the waiting process
    SYSCALL(SENDMESSAGE, (unsigned int)p, 0, 0);
    // wait for the process to release the mutual exclusion
    SYSCALL(RECEIVEMESSAGE, (unsigned int)p, 0, 0);
  }
}

void InitiatorProcess() {
  initiator_pcb = current_process;
  // TODO: initialize the Swap Pool

  // initialize the state for the mutex pcb
  STST(&mutex_state);
  mutex_state.reg_sp = mutex_state.reg_sp - PAGESIZE / 4;
  mutex_state.pc_epc = (memaddr)swap_mutex;
  mutex_state.status |= MSTATUS_MIE_MASK | MSTATUS_MPP_M;
  mutex_state.mie = MIE_ALL;
  swap_mutex_pcb = create_process(&mutex_state, NULL);

  // TODO: non ha molto senso quello che segue: Each (potentially) sharable
  // peripheral I/O device can have a process for it. These process will be used
  // to receive complex requests (i.e. to write of a string to a terminal) and
  // request the correct DoIO service to the SSI (this feature is optional and
  // can be delegated directly to the SST processes to simplify the project).

  // create the 8 SST for the Uprocs
  for (int i = 0; i < UPROCMAX; i++) {
    // TODO: substitute sst_start with correct function and check stack pointer,
    // check Kernel mode
    STST(&sst_state[i]);
    sst_state[i].reg_sp = sst_state[i].reg_sp - PAGESIZE / 4;
    sst_state[i].pc_epc = (memaddr)sst_start;
    sst_state[i].status |= MSTATUS_MIE_MASK | MSTATUS_MPP_M;
    sst_state[i].mie = MIE_ALL;
    sst_pcbs[i] = create_process(&sst_state[i], NULL);
  }

  //  Wait for 8 messages, that should be send when each SST is terminated.
  for (int i = 0; i < UPROCMAX; i++) {
    SYSCALL(RECEIVEMESSAGE, (unsigned int)sst_pcbs[i], 0, 0);
  }

  // reduce process count to 1 by itself and its children (the mutex)
  ssi_payload_t term_process_payload = {
      .service_code = TERMPROCESS,
      .arg = (void *)NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
          (unsigned int)(&term_process_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
  // if it gets to this point the process was not correctly killed, PANIC
  PANIC();
}
