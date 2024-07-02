#include "./headers/sst.h"
#include "../headers/types.h"
#include "../phase2/headers/initial.h"
#include "./headers/initProc.h"
#include "./headers/sysSupport.h"
#include "./headers/vmSupport.h"
#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>

typedef unsigned int devregtr;

extern pcb_t *ssi_pcb;

pcb_t *uprocs[UPROCMAX];
state_t uproc_state[UPROCMAX];

// support function to setup each process' page table
void setUpPageTable(support_t *uproc) {
  for (int i = 0; i < USERPGTBLSIZE - 1; i++) {
    uproc->sup_privatePgTbl[i].pte_entryHI =
        (0x80000000 + i * PAGESIZE) + (uproc->sup_asid << ASIDSHIFT);
    uproc->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
  }
  // set the last VPN to 0xBFFFF000
  uproc->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI =
      (USERSTACKTOP - PAGESIZE) + (uproc->sup_asid << ASIDSHIFT);
  uproc->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;
}

void SST_service();

/*
 * SST creation
 * first the corresponding child u-proc is created
 * then the SST waits for service requests from its child process
 */
void SST_entry_point() {
  // obtain the asid
  support_t *proc_sup = getSupStruct();
  int i = proc_sup->sup_asid - 1;
  // initial proc state
  uproc_state[i].reg_sp = USERSTACKTOP;
  uproc_state[i].pc_epc = UPROCSTARTADDR;
  // set all interrupts on and user mode (its mask is 0x0)
  uproc_state[i].status = MSTATUS_MPIE_MASK;
  uproc_state[i].mie = MIE_ALL;
  // set entry hi asid to i
  uproc_state[i].entry_hi = (i + 1) << ASIDSHIFT;

  // initialize uproc support struct
  memaddr ramtop;
  RAMTOP(ramtop);
  // give the top ram frames as the stack pointers for the exception handlers
  memaddr initial_stack_frame = ramtop - (2 * PAGESIZE);
  proc_sup->sup_exceptContext[0] =
      (context_t){.pc = (memaddr)TLB_ExceptionHandler,
                  .status = STATUS_INTERRUPT_ON_NEXT,
                  .stackPtr = initial_stack_frame - i * PAGESIZE};

  proc_sup->sup_exceptContext[1] = (context_t){
      .pc = (memaddr)generalExceptionHandler,
      .status = STATUS_INTERRUPT_ON_NEXT,
      .stackPtr = initial_stack_frame - i * PAGESIZE + HALFPAGESIZE};
  // initialize pgTbl
  setUpPageTable(proc_sup);

  uprocs[i] = create_process(&uproc_state[i], proc_sup);

  // after creating the uproc start waiting for its messages
  SST_service(i);
}

void SSTRequest(pcb_t *sender, int service, void *arg, int number);

// main loop to support the uproc
void SST_service(int i) {
  while (1) {
    ssi_payload_t *payload;
    pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, (unsigned int)uprocs[i],
                                     (unsigned int)&payload, 0);
    SSTRequest(sender, payload->service_code, payload->arg, i);
  }
}

extern pcb_t *initiator_pcb;

// get time passed since the system booted
void getTOD(pcb_t *sender) {
  unsigned int tod;
  STCK(tod);
  SYSCALL(SENDMESSAGE, (unsigned int)sender, tod, 0);
}

// Terminate SST and its uproc
void terminateSST() {
  // free the memory frames occupied by corresponding uproc
  support_t *proc_sup = getSupStruct();
  freeProcFrames(proc_sup->sup_asid);

  // sending message to initProc to communicate the termination of the SST
  SYSCALL(SENDMESSAGE, (unsigned int)initiator_pcb, 0, 0);
  // terminate sst (current process) and its progeny
  ssi_payload_t term_process_payload = {
      .service_code = TERMPROCESS,
      .arg = (void *)NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
          (unsigned int)(&term_process_payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

void writeOnDevice(pcb_t *sender, void *arg, unsigned int controller_number,
                   unsigned int dev_type) {
  // get the controller based on the dev type
  devreg_t *controller = (devreg_t *)DEV_REG_ADDR(dev_type, controller_number);
  sst_print_t *print_payload = arg;
  char *string = print_payload->string;
  devregtr status;
  // setup the support variables based on the type of device
  unsigned int ok_code = DEVREADY, additional_char = 0;
  memaddr *command_addr = &controller->dtp.command;
  if (dev_type == IL_TERMINAL) {
    command_addr = &controller->term.transm_command;
    ok_code = RECVD;
  }

  for (int i = 0; i < print_payload->length; i++) {
    if (dev_type == IL_TERMINAL)
      additional_char = (((devregtr)*string) << 8);
    else
      controller->dtp.data0 = (devregtr)*string;
    // create the payload
    ssi_do_io_t do_io = {.commandAddr = command_addr,
                         .commandValue = PRINTCHR | additional_char};
    ssi_payload_t payload = {.service_code = DOIO, .arg = &do_io};
    // SSI DOIO
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&status, 0);

    // check that status is not an error
    if ((status & STATMASK) != ok_code)
      programTrapExceptionHandler();

    // get the next char
    string++;
  }

  // write to the sender that is awaiting an empty response
  SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
}

void SSTRequest(pcb_t *sender, int service, void *arg, int number) {
  switch (service) {
  case GET_TOD:
    getTOD(sender);
    break;
  case TERMINATE:
    terminateSST();
    break;
  case WRITEPRINTER:
    writeOnDevice(sender, arg, number, IL_PRINTER);
    break;
  case WRITETERMINAL:
    writeOnDevice(sender, arg, number, IL_TERMINAL);
    break;
  }
}
