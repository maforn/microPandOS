#include "./headers/pcb.h"
#include "./headers/msg.h"


extern void test();

int main() {
	// ATT: per le librerie di uriscv guardare /urs/local/inlclude/uriscv
	// (1)
	int process_count, soft_block_count = 0;
	struct list_head ready_queue;
	mkEmprtyProcQ(&ready_queue);
	pcb_t *current_process;
	// TODO: Blocked PCBs list necessario non ho capito
	
	// (2)
	// TODO: controllare funzioni l'assegnamento dell'indirizzo
	memaddr *passup_addr = (memaddr *)PASSUPVECTOR;
	struct passupvector *pass_up_vector = (struct passupvector *)passup_addr;
	pass_up_vector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
	pass_up_vector->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
	pass_up_vector->exception_handler = (memaddr)exceptionHandler;
	pass_up_vector->exception_stackPtr = (memaddr)KERNELSTACK;
	
	// (3)
	initPcbs();
	initMsgs();

	// (4): viene fatto successivamente o è già stato inizializzato
	
	// (5)
	// TODO: controllare sia la funzione giusta (presa da const.h)
	LDIT(PSECOND);

	// (6)
	pcb_t *first_process = allocPcb();
	insertProcQ(&ready_queue, pcb_t);
	process_count = 1;
	// TODO: ricontrollare i bits di interrupt e in generale la correttezza
	first_process->p_s->mie = (1 << 16) - 1;
	// TODO: capire cose è kernel mode, SP to RAMTOP,  
	// questo LDCXT()?
	// TODO: create function
	first_process->p_s->pc_epc = (memaddr)SSI_function_entry_point
	// process tree to NULL already done by allocPcb()
	// p_time = 0 by allocPcb again, as well as p_supportStruct
	
	// (7)
	pcb_t *second_process = allocPcb();
	insertProcQ(&ready_queue, pcb_t);
	process_count++;
	// TODO: inizializzare simile a sopra ma tanto non stiamo capendo
	
	// (8)
	// TODO: call the Scheduler
}
