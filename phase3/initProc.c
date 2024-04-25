#include "../headers/types.h"
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

void setupPageTable(support_t *uproc) {
  for (int i = 0; i < USERPGTBLSIZE; i++) {
    // TODO: check shift
    uproc->sup_privatePgTbl[i].pte_entryHI =
        ((UPROCSTARTADDR + i * PAGESIZE) << VPNSHIFT) +
        (uproc->sup_asid << ASIDSHIFT);
    uproc->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
  }
  // set the last VPN to 0xBFFFF000
  uproc->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI =
      ((USERSTACKTOP - PAGESIZE) << VPNSHIFT) + (uproc->sup_asid << ASIDSHIFT);
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

  // TODO: controllare come mai e controllare che vada bene in Kernel Mode
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

    /* this part is to initialize the Uproc and should be in the SST
    uproc_state.reg_sp = USERSTACKTOP;
    uproc_state.pc_epc = UPROCSTARTADDR;
    // set all interrupts on and user mode (its mask is 0x0)
    uproc_state.status = MSTATUS_MIE_MASK;
    uproc_state.mie = MIE_ALL;
    // TODO: check
    // set entry hi asid to i
    uproc_state.entry_hi = (i + 1) << ASIDSHIFT;
    uproc_sup[i].sup_asid = i + 1;
    // TODO: add with other code and check, because I am really confused
    memaddr ramtop;
    RAMTOP(ramtop);
    uproc_sup[i].sup_exceptContext[0] = {.pc = &support_level_TLB_handler,
                                         .status =
                                             MSTATUS_MIE_MASK | MSTATUS_MPP_M,
                                         .stackPtr = ramtop - PAGESIZE};

    uproc_sup[i].sup_exceptContext[1] = {.pc = &support_level_general_handler,
                                         .status =
                                             MSTATUS_MIE_MASK | MSTATUS_MPP_M,
                                         .stackPtr = ramtop - PAGESIZE};
    setupPageTable(&uproc_sup[i]);
    create_process(&uproc_state, &uproc_sup[i]);*/
  }

  //  Wait for 8 messages, that should be send when each SST is terminated.
  for (int i = 0; i < UPROCMAX; i++) {
    SYSCALL(RECEIVEMESSAGE, (unsigned int)sst_pcbs[i], 0, 0);
  }

  // reduce process count to 1 by killing the mutex
  ssi_payload_t term_process_payload = {
      .service_code = TERMPROCESS,
      .arg = (void *)swap_mutex_pcb,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
          (unsigned int)(&term_process_payload), 0);
  // TODO: I do not need to wait for a response right?
  // SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}
