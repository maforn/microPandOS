
void schedule() {
	// TODO: capire cosa vuol dire round-robin
	current_process = removeProcQ(ready_queue);
	// TODO: TIMEEEEEEEEEEEEEEER?????????????????????
	setTIMER(TIMESLICE);
	// Andrebbe passato come pointer, è corretto?
	LDST(current_process->p_s.status);

	if (emptyProcQ(ready_queue)) {
		// TODO: Controllare se è così che si controlla che SSI è l'unico vivo
		if (process_count == 1 && current_process->p_s.pc_epc == (memaddr)SSI_function_entry_point)
			HALT();
		else if (process_count > 0 && soft_block_count > 0)
			// TODO: riabilitare gli interrupt
			WAIT();
		// deadlock
		else if (process_count > 0 && soft_block_count == 0)
			PANIC();
	}		
}
