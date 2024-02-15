#include "./headers/pcb.h"
#include "./headers/msg.h"

// (1)
int process_count, soft_block_count;
struct list_head ready_queue;
pcb_t *current_process;
// TODO: Blocked PCBs list necessario non ho capito


#include "./scheduler.c"
#include "./exceptions.c"
#include "./ssi.c"

extern void test();


int main() {
	// ATT: per le librerie di uriscv guardare /urs/local/inlclude/uriscv
	
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

	// (4)
	process_count = 0;
	soft_block_count = 0;
	mkEmptyProcQ(&ready_queue);

	// (5)
	// TODO: controllare sia la funzione giusta (presa da const.h)
	LDIT(PSECOND);

	// (6)
	pcb_t *first_process = allocPcb();
	insertProcQ(&ready_queue, first_process);
	process_count++;
	// TODO: ricontrollare i bits di interrupt e in generale la correttezza
	first_process->p_s.mie = (1 << 16) - 1; // tutti a 1
	first_process->p_s.mie = (1 << 11) + (1 << 7) + (1 << 3); // solo quelli non nulli e non 0

	// TODO: ricontrollare che sia giusto
	first_process->p_s.status = 0;
	// Obtain ramtop with the macro
	memaddr ramtop;
	RAMTOP(ramtop);
	first_process->p_s.gpr[28] = ramtop;
	
	// TODO: create function SSI_function...
	first_process->p_s.pc_epc = (memaddr)SSI_function_entry_point;
	
	// process tree to NULL already done by allocPcb()
	// p_time = 0 by allocPcb again, as well as p_supportStruct
	
	// (7)
	pcb_t *second_process = allocPcb();
	insertProcQ(&ready_queue, second_process);
	process_count++;
	// TODO: controllare come sopra
	// TODO: capire se esiste il bit per il local timer 
	first_process->p_s.status = 1 << 27;
	// TODO: assumo che FRAMESIZE siano i frame della ram e siano PAGESIZE 
	first_process->p_s.gpr[28] = ramtop - (2 * PAGESIZE);
	
	// TODO: create function SSI_function...
	first_process->p_s.pc_epc = (memaddr)test;
	

	// (8)
	// TODO: call the Scheduler
}
