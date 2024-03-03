#include "./headers/ssi.h"
#include "../phase1/headers/pcb.h"
#include "./headers/initial.h"
#include <uriscv/liburiscv.h>

/**
 * This function will do basic checks for deadlock or waiting condition and then load the next
 * process in the ready_queue as the current one 
*/
void schedule() {
	if (emptyProcQ(&ready_queue)) {
		// TODO: Controllare se è così che si controlla che SSI è l'unico vivo
		// if there is only one process and it's the SSI, then HALT the system (test is finished)
		if (process_count == 1 && current_process->p_pid == 0)
			HALT();
		// if there is no process and more then one are blocked then they are waiting for an interrupt
		else if (process_count > 0 && soft_block_count > 0) {
			// set local timer so that it does not get unlocked before the interrupt of the Interval Timer
			setTIMER(PSECOND);
			// TODO: CONTROLLARE correttezza
			// set interrupt mask as all on
			setMIE(MIE_ALL);
			setSTATUS(STATUS_INTERRUPT_ON_NOW);
			// wait for an interrupt
			WAIT();
		}
		// if there is at least one process but the ready queue is empty and no process is blocked waiting
		// for an interrupt it's a deadlock: panic
		else if (process_count > 0 && soft_block_count == 0)
			PANIC();
	}

	current_process = removeProcQ(&ready_queue);
	// set PLT
	setTIMER(TIMESLICE);
	// load the current process as the active one
	LDST(&(current_process->p_s));
}
