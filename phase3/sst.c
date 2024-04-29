#include "../headers/types.h"
#include <stdlib.h>
#include <uriscv/liburiscv.h>

//salviamo in RAM stack_tlbExceptHandler e stack_generalExceptHandler di ogni u-proc
//a partire da PENULTIMATE_RAM_FRAME... 
//ci calcoliamo, a seconda del processo, l'indirizzo di memoria, sapendo che i due stack occupano
//entrambi mezza pagesize, quindi insieme avranno la size di una pagesize
//-> avremo la prima page per gli stack del primo u-proc, la seconda per gli stack del secondo e così via...

state_t* get_initial_proc_state(unsigned short procNumber) {
	// TODO: check if values are correct
	state_t* uproc_state = malloc(sizeof(state_t));
	uproc_state->reg_sp = USERSTACKTOP;
    	uproc_state->pc_epc = UPROCSTARTADDR;
    	// set all interrupts on and user mode (its mask is 0x0)
    	uproc_state->status = MSTATUS_MIE_MASK;
    	uproc_state->mie = MIE_ALL;
    	// set entry hi asid to i
    	uproc_state->entry_hi = procNumber << ASIDSHIFT;
	return uproc_state;
}

void setUpPageTable(support_t *uproc) {
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

support_t* get_proc_support_struct(unsigned short procNumber) {
	support_t* uproc_sup = malloc(sizeof(support_t));
	//initialize asid
	uproc_sup->sup_asid = procNumber;
	
	//initialize sup_exceptContext 
	memaddr ramtop;
	RAMTOP(ramtop);
	memaddr PENULTIMATE_RAM_FRAME = ramtop - PAGESIZE;
	//TODO: set pc to address of support_level_tlb_handler (I put 0 as placeholder)
	uproc_sup->sup_exceptContext[0] = (context_t) {.pc = 0, .status = MSTATUS_MIE_MASK + MSTATUS_MPP_M,
	.stackPtr = PENULTIMATE_RAM_FRAME - (procNumber-1)*PAGESIZE};
	//TODO: set pc to address of support_level_general_handler (I put 0 as placeholder)	
	uproc_sup->sup_exceptContext[1] = (context_t) {.pc = 0, .status = MSTATUS_MPP_M + MSTATUS_MIE_MASK,
	.stackPtr = PENULTIMATE_RAM_FRAME - (procNumber-1)*PAGESIZE + 0.5*PAGESIZE};

	//initialize pgTbl
	setUpPageTable(uproc_sup);
	return uproc_sup;
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

/*
 * SST creation
 * first the corresponding child u-proc is created
 * then the SST waits for service requests from its child process
*/
void create_SST(unsigned short procNumber) {
	//pointer to initial processor state for the u-proc	
	state_t* uproc_proc_state = get_initial_proc_state(procNumber); 
	
	//pointer to an initialized support structure for the u-proc
	support_t* uproc_sup = get_proc_support_struct(procNumber);

	//launch u-proc (create_process request to SSI)i
	pcb_t* uproc = create_process(uproc_proc_state, uproc_sup); 
}
