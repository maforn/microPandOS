#include "./headers/initProc.h"
#include "../phase2/headers/initial.h"
#include "./headers/sst.h"
#include "./headers/sysSupport.h"
#include "./headers/vmSupport.h"
#include <uriscv/liburiscv.h>

pcb_PTR initiator_pcb, swap_mutex_pcb, sst_pcbs[UPROCMAX];

extern pcb_t *ssi_pcb;

state_t sst_state[UPROCMAX], mutex_state;

// support function to create a process given its state and support struct
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

static support_t uproc_sup_array[UPROCMAX];

// function executed by the mutex pcb
void swap_mutex() {
  pcb_t *p;
  int *i;
  while (1) {
    // use i to determinate if it is a forced release for mutex message on process termination
    i = 0;
    // wait for someone to request the mutual exlusion
    p = (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&i, 0);
    // if i != 0 then it's a forced release, do not enter in mutex
    if (i == 0) {
      // give the ok to the waiting process
      SYSCALL(SENDMESSAGE, (unsigned int)p, 0, 0);
      // wait for the process to release the mutual exclusion
      SYSCALL(RECEIVEMESSAGE, (unsigned int)p, 0, 0);
    }
  }
}

// entry point of the first process initiated after the ssi
void InitiatorProcess() {
  initiator_pcb = current_process;

  // Initialize the Swap Pool
  initSwapStructs();
  // initialize the the mutex pcb
  STST(&mutex_state);
  mutex_state.reg_sp = mutex_state.reg_sp - QPAGE;
  mutex_state.pc_epc = (memaddr)swap_mutex;
  mutex_state.status = STATUS_INTERRUPT_ON_NEXT;
  mutex_state.mie = MIE_ALL;
  swap_mutex_pcb = create_process(&mutex_state, NULL);
  unsigned int last_stack = mutex_state.reg_sp;

  // clear the first random entry for the TLB
  TLBWR();
  TLBCLR();
  // create the SSTs for the Uprocs
  for (int i = 0; i < UPROCMAX; i++) {
    STST(&sst_state[i]);
    sst_state[i].reg_sp = last_stack - QPAGE;
    sst_state[i].pc_epc = (memaddr)SST_entry_point;
    sst_state[i].status = STATUS_INTERRUPT_ON_NEXT;
    sst_state[i].mie = MIE_ALL;
    sst_state[i].entry_hi = (i + 1) << ASIDSHIFT;
    last_stack = sst_state[i].reg_sp;
    // SST shares the same support struct of its uproc
    uproc_sup_array[i].sup_asid = i + 1;

    sst_pcbs[i] = create_process(&sst_state[i], &uproc_sup_array[i]);
  }

  //  Wait for messages, that each SST should send when it is terminated.
  for (int i = 0; i < UPROCMAX; i++) {
    SYSCALL(RECEIVEMESSAGE, (unsigned int)sst_pcbs[i], 0, 0);
  }

  // reduce process count to 1 by killing itself and its children (the mutex)
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
