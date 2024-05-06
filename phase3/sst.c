#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include "../phase2/headers/ssi.h"
#include <uriscv/arch.h>

#define STATUS_INTERRUPT_ON_NEXT (MSTATUS_MPIE_MASK + MSTATUS_MPP_M)
#define TERMSTATMASK 0xFF

typedef unsigned int devregtr;

//salviamo in RAM stack_tlbExceptHandler e stack_generalExceptHandler di ogni u-proc
//a partire da PENULTIMATE_RAM_FRAME... 
//ci calcoliamo, a seconda del processo, l'indirizzo di memoria, sapendo che i due stack occupano
//entrambi mezza pagesize, quindi insieme avranno la size di una pagesize
//-> avremo la prima page per gli stack del primo u-proc, la seconda per gli stack del secondo e così via...

void setUpPageTable(support_t *uproc) {
  for (int i = 0; i < USERPGTBLSIZE-1; i++) {
    // TODO: check shift
    uproc->sup_privatePgTbl[i].pte_entryHI =
        ((UPROCSTARTADDR + i * PAGESIZE) << VPNSHIFT) +
        (uproc->sup_asid << ASIDSHIFT);
    uproc->sup_privatePgTbl[i].pte_entryLO = DIRTYON;
  }
  // set the last VPN to 0xBFFFF000
  uproc->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryHI =
      ((USERSTACKTOP - PAGESIZE) << VPNSHIFT) + (uproc->sup_asid << ASIDSHIFT);
  uproc->sup_privatePgTbl[USERPGTBLSIZE - 1].pte_entryLO = DIRTYON;
}

extern pcb_t* ssi_pcb;

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

extern support_t uproc_sup_array[UPROCMAX];
pcb_t* uproc;
//TODO: change name of uproc_sup in initial.c to uproc_sup_array

/*
 * SST creation
 * first the corresponding child u-proc is created
 * then the SST waits for service requests from its child process
*/
void create_SST(unsigned short procNumber) {
	//initial proc state
	state_t uproc_state;
	uproc_state.reg_sp = USERSTACKTOP;
    	uproc_state.pc_epc = UPROCSTARTADDR;
    	// set all interrupts on and user mode (its mask is 0x0)
    	uproc_state.status = MSTATUS_MPIE_MASK;
    	uproc_state.mie = MIE_ALL;
    	// set entry hi asid to i
    	uproc_state.entry_hi = procNumber << ASIDSHIFT;
	
	//proc support structure (working on the reference of array's elements)
	support_t* uproc_sup = &uproc_sup_array[procNumber-1];
	//initialize asid
	uproc_sup->sup_asid = procNumber;
	//initialize sup_exceptContext 
	memaddr ramtop;
	RAMTOP(ramtop);
	memaddr PENULTIMATE_RAM_FRAME = ramtop - PAGESIZE;
	//TODO: set pc to address of support_level_tlb_handler (I put 0 as placeholder)
	uproc_sup->sup_exceptContext[0] = (context_t) {.pc = 0, .status = STATUS_INTERRUPT_ON_NEXT,
	.stackPtr = PENULTIMATE_RAM_FRAME - (procNumber-1)*PAGESIZE};
	//TODO: set pc to address of support_level_general_handler (I put 0 as placeholder)	
	uproc_sup->sup_exceptContext[1] = (context_t) {.pc = 0, .status = STATUS_INTERRUPT_ON_NEXT,
	.stackPtr = PENULTIMATE_RAM_FRAME - (procNumber-1)*PAGESIZE + 0.5*PAGESIZE};
	//initialize pgTbl
	setUpPageTable(uproc_sup);

	uproc = create_process(&uproc_state, uproc_sup); 
}

void SSTRequest(pcb_t* sender, int service, void* arg);

void SST_service_entry_point() {
	while (1) {
		ssi_payload_t *payload;
		pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, (unsigned int)uproc, (unsigned int)&payload, 0);
		SSTRequest(sender, payload->service_code, payload->arg);
	}
}
	
extern pcb_t* initiator_pcb;

//Terminate SST and child
void terminateSST() {
	//sending message to initProc to communicate the termination of the SST
	SYSCALL(SENDMESSAGE, (unsigned int)initiator_pcb, 0,0);
	//terminate sst (current process) and its progeny
	ssi_payload_t term_process_payload = {
		.service_code = TERMPROCESS,
		.arg = (void *)NULL,
	};
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&term_process_payload), 0);
}

void writeOnDevice(unsigned short device_number, pcb_t* sender, void* arg) {
	unsigned short controller_number = sender->p_supportStruct->sup_asid - 1;
	devregtr* controller = (devregtr *)DEV_REG_ADDR(device_number + DEV_IL_START, controller_number);
	char* string = arg;
	devregtr status;

	while (*string != EOS){
		devregtr value = PRINTCHR | (((devregtr)*string) << 8);
		ssi_do_io_t do_io = {
			.commandAddr = controller,
			.commandValue = value,
		};
		ssi_payload_t payload = {
			.service_code = DOIO,
			.arg = &do_io,
		};
		SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
		SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status), 0);

		if ((status & TERMSTATMASK) != RECVD)
			PANIC();

		string++;
    }
}

void SSTRequest(pcb_t* sender, int service, void* arg) {
	switch (service) {
		case GET_TOD:
			long unsigned int tod;
			STCK(tod);
			SYSCALL(SENDMESSAGE, sender, &tod, 0);
			break;
		case TERMINATE:
			terminateSST();
			break;
		case WRITEPRINTER:
			writeOnDevice(DEV_N_PRINTER, sender, arg);
			break;
		case WRITETERMINAL:
			writeOnDevice(DEV_N_TERMINAL, sender, arg);
			break;
	}
}