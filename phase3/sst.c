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

static pcb_t *uproc;

void SST_service();

void setUpPageTable(support_t *uproc) {
  for (int i = 0; i < USERPGTBLSIZE - 1; i++) {
    // TODO: check shift
    uproc->sup_privatePgTbl[i].pte_entryHI =
        (0x80000000 + i * PAGESIZE) +
        (uproc->sup_asid << ASIDSHIFT);
    uproc->sup_privatePgTbl[i].pte_entryLO = DIRTYON | GLOBALON;
  }
  // set the last VPN to 0xBFFFF000
  uproc->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI =
      (USERSTACKTOP - PAGESIZE) + (uproc->sup_asid << ASIDSHIFT);
  uproc->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON | GLOBALON;
}

/*
 * SST creation
 * first the corresponding child u-proc is created
 * then the SST waits for service requests from its child process
 */
void SST_entry_point() {
  // obtain the asid
  support_t *proc_sup = getSupStruct();
  int procNumber = proc_sup->sup_asid;
  // initial proc state
  state_t uproc_state;
  uproc_state.reg_sp = USERSTACKTOP;
  uproc_state.pc_epc = UPROCSTARTADDR;
  // set all interrupts on and user mode (its mask is 0x0)
  uproc_state.status = MSTATUS_MPIE_MASK;
  uproc_state.mie = MIE_ALL;
  // set entry hi asid to i
  uproc_state.entry_hi = procNumber << ASIDSHIFT;

  // initialize uproc support struct
  memaddr ramtop;
  RAMTOP(ramtop);
  memaddr PENULTIMATE_RAM_FRAME = ramtop - PAGESIZE;
  proc_sup->sup_exceptContext[0] =
      (context_t){.pc = (memaddr)TLB_ExceptionHandler,
                  .status = STATUS_INTERRUPT_ON_NEXT,
                  .stackPtr = PENULTIMATE_RAM_FRAME - procNumber * PAGESIZE};

  proc_sup->sup_exceptContext[1] = (context_t){
      .pc = (memaddr)generalExceptionHandler,
      .status = STATUS_INTERRUPT_ON_NEXT,
      .stackPtr = PENULTIMATE_RAM_FRAME - procNumber * PAGESIZE + HALFPAGESIZE};
  // initialize pgTbl
  setUpPageTable(proc_sup);

  uproc = create_process(&uproc_state, proc_sup);

  SST_service();
}

void SSTRequest(pcb_t *sender, int service, void *arg);

void SST_service() {
  while (1) {
    ssi_payload_t *payload;
    pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, (unsigned int)uproc,
                                     (unsigned int)&payload, 0);
    SSTRequest(sender, payload->service_code, payload->arg);
  }
}

extern pcb_t *initiator_pcb;

void getTOD(pcb_t *sender) {
  long unsigned int tod;
  STCK(tod);
  SYSCALL(SENDMESSAGE, (unsigned int)sender, tod, 0);
}

// Terminate SST and child
void terminateSST() {
  // sending message to initProc to communicate the termination of the SST
  SYSCALL(SENDMESSAGE, (unsigned int)initiator_pcb, 0, 0);
  // terminate sst (current process) and its progeny
  ssi_payload_t term_process_payload = {
      .service_code = TERMPROCESS,
      .arg = (void *)NULL,
  };
  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
          (unsigned int)(&term_process_payload), 0);
}

void writeOnPrinter(pcb_t *sender, void *arg) {
	unsigned short controller_number = sender->p_supportStruct->sup_asid - 1;
	devreg_t *controller = (devreg_t *) DEV_REG_ADDR(IL_PRINTER, controller_number);
	char *string = arg;	
	devregtr status;

	while (*string != EOS) {
		controller->dtp.data0 = (devregtr) *string;
		ssi_do_io_t do_io = {
			.commandAddr = &controller->dtp.command,
			.commandValue = PRINTCHR
		};
		ssi_payload_t payload = {
			.service_code = DOIO,
			.arg = &do_io
		};
		SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
		SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&status, 0);
	
		// TODO: check if the commented status checking would be correct
		// DEV_READY would be defined as 1
		/*if (status != DEV_READY) 
			PANIC();
		*/

		string++;
	}

	// write to the sender that is awaiting an empty response
	SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
}

void writeOnTerminal(pcb_t *sender, void *arg) {
  unsigned short controller_number = sender->p_supportStruct->sup_asid - 1;
  devreg_t *controller =(devreg_t *)DEV_REG_ADDR(IL_TERMINAL, controller_number);
  char *string = arg;
  devregtr status;

  while (*string != EOS) {
    ssi_do_io_t do_io = {
        .commandAddr = &controller->term.transm_command,
        .commandValue = PRINTCHR | (((devregtr)*string) << 8),
    };
    ssi_payload_t payload = {
        .service_code = DOIO,
        .arg = &do_io,
    };
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status), 0);

    if ((status & TERMSTATMASK) != RECVD)
      // TODO: PANIC? or generalExceptionHandler?
      PANIC();

    string++;
  }

  // write to the sender that is awaiting an empty response
  SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
}

void SSTRequest(pcb_t *sender, int service, void *arg) {
  switch (service) {
  case GET_TOD:
    getTOD(sender);
    break;
  case TERMINATE:
    terminateSST();
    break;
  case WRITEPRINTER:
    writeOnPrinter(sender, arg);
    break;
  case WRITETERMINAL:
    writeOnTerminal(sender, arg);
    break;
  }
}
