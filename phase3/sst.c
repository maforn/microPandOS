#include "../headers/types.h"
#include <uriscv/liburiscv.h>

#define STATUS_INTERRUPT_ON_NEXT (MSTATUS_MPIE_MASK + MSTATUS_MPP_M)

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
	
	//proc support structure
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

	pcb_t* uproc = create_process(&uproc_state, uproc_sup); 
}
