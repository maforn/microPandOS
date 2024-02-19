#include "./headers/ssi.h"
#include "../phase1/headers/pcb.h"
#include "./headers/initial.h"
#include <uriscv/liburiscv.h>

void schedule() {

	if (emptyProcQ(&ready_queue)) {
		// TODO: Controllare se è così che si controlla che SSI è l'unico vivo
		if (process_count == 1 && current_process->p_s.reg_sp == (memaddr)SSI_function_entry_point)
			HALT();
		else if (process_count > 0 && soft_block_count > 0) {
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
	// Andrebbe passato come pointer, è corretto?
	LDST(&(current_process->p_s));
}
