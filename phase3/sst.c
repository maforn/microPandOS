#include "../headers/types.h"
//TODO: add needed includes

state_t* get_initial_proc_state(unsigned short index) {
	// TODO: check if correct
	state_t proc_state;
	proc_state.reg_sp = USERSTACKTOP;
    	proc_state.pc_epc = UPROCSTARTADDR;
    	// set all interrupts on and user mode (its mask is 0x0)
    	proc_state.status = MSTATUS_MIE_MASK;
    	proc_state.mie = MIE_ALL;
    	// set entry hi asid to i
    	proc_state.entry_hi = index << ASIDSHIFT;

}

/*
 * SST creation
 * first the corresponding child u-proc is initialized and created
 * then the SST waits for service requests from its child process
*/
void create_SST(unsigned short index) {
	//pointer to initial processor state for the u-proc	
	state_t* initial_proc_state = get_initial_proc_state(index); 
	
	//pointer to an initialized support structure for the u-proc
	
	//launch u-proc (create_process request to SSI)
}
