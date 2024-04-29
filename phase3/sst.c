#include "../headers/types.h"
#include <stdlib.h>

#define PENULTIMATE_RAM_FRAME (RAMTOP - PAGESIZE)

//salveremo in RAM stack_tlbExceptHandler e stack_generalExceptHandler di ogni u-proc
//a partire da PENULTIMATE_RAM_FRAME... 
//ci calcoliamo, a seconda dell'index, l'indirizzo di memoria, sapendo che i due stack occupano
//entrambi mezza pagesize, quindi insieme avranno la size di una pagesize
//-> avremo la prima page per gli stack del primo u-proc, la seconda per gli stack del secondo e così via...

state_t* get_initial_proc_state(unsigned short index) {
	// TODO: check if values are correct
	state_t* uproc_state = malloc(sizeof(state_t));
	uproc_state->reg_sp = USERSTACKTOP;
    	uproc_state->pc_epc = UPROCSTARTADDR;
    	// set all interrupts on and user mode (its mask is 0x0)
    	uproc_state->status = MSTATUS_MIE_MASK;
    	uproc_state->mie = MIE_ALL;
    	// set entry hi asid to i
    	uproc_state->entry_hi = index << ASIDSHIFT;
	return uproc_state;
}

support_t* get_proc_support_struct(unsigned short index) {
	support_t* uproc_sup = malloc(sizeof(support_t));
	//initialize asid
	uproc_sup->sup_asid = index;
	//initialize sup_exceptContext 
	uproc_sup->sup_exceptContext[0] = {.pc = &support_level_TLB_handler, .status = MSTATUS_MIE_MASK + MSTATUS_MPP_M,
	.stackPtr = PENULTIMATE_RAM_FRAME - (index-1)*PAGESIZE};
	uproc_sup->sup_exceptContext[1] = {.pc = &support_level_general_handler, .status = MSTATUS_MPP_M + MSTATUS_MIE_MASK,
	.stackPtr = PENULTIMATE_RAM_FRAME - (index-1)*PAGESIZE + 0.5*PAGESIZE};
	//TODO: initialize pgTbl
	return uproc_sup;
}


/*
 * SST creation
 * first the corresponding child u-proc is initialized and created
 * then the SST waits for service requests from its child process
*/
void create_SST(unsigned short index) {
	//pointer to initial processor state for the u-proc	
	state_t* uproc_proc_state = get_initial_proc_state(index); 
	
	//pointer to an initialized support structure for the u-proc
	support_t* uproc_sup = get_proc_support_struct(index);

	//launch u-proc (create_process request to SSI)
}
