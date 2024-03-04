#include "./headers/ssi.h"
#include "../phase1/headers/pcb.h"
#include "./headers/initial.h"
#include <uriscv/liburiscv.h>


// TODO: check condition
int ssiOnlyProcess(){
	return process_count == 1 && (current_process == ssi_pcb || headProcQ(&ready_queue) == ssi_pcb
		|| (soft_block_count == 1 && ssi_pcb->blocked==1));
}

void schedule() {
	if (emptyProcQ(&ready_queue)) {
		// TODO: Controllare se è così che si controlla che SSI è l'unico vivo
		if (ssiOnlyProcess())
			HALT();
		else if (process_count > 0 && soft_block_count > 0) {
			setTIMER(PSECOND);
			// TODO: CONTROLLARE correttezza
			setMIE(MIE_ALL);
			setSTATUS(STATUS_INTERRUPT_ON_NOW);
			WAIT();
		}
		// deadlock
		else if (process_count > 0 && soft_block_count == 0)
			PANIC();
	}

	current_process = removeProcQ(&ready_queue);
	setTIMER(TIMESLICE);
	LDST(&(current_process->p_s));
}
