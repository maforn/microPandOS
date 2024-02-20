#include "../phase1/headers/msg.h"

// (1)
// In the header file
int process_count, soft_block_count;
struct list_head ready_queue;
pcb_t *current_process;
struct list_head blocked_pcbs[SEMDEVLEN][2];
// TODO: Blocked PCBs controllare la correttezza

#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include "./headers/exceptions.h"
#include "./headers/ssi.h"

//extern void test();
void test() {
	while (1) {}
}

int main() {
	// ATT: per le librerie di uriscv guardare /urs/local/include/uriscv
	// (2)
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
	for (int i = 0; i < SEMDEVLEN; i++) {
		mkEmptyProcQ(&blocked_pcbs[i][0]);
		mkEmptyProcQ(&blocked_pcbs[i][1]);
	}

	// (5)
	// TODO: controllare sia la funzione giusta (presa da const.h)
	LDIT(PSECOND);

	// (6)
	pcb_t *first_process = allocPcb();
	insertProcQ(&ready_queue, first_process);
	process_count++;
	first_process->p_s.mie = MIE_ALL;

	// TODO: ricontrollare che sia giusto
	first_process->p_s.status = STATUS_INTERRUPT_ON_NEXT;
	// Obtain ramtop with the macro
	memaddr ramtop;
	RAMTOP(ramtop);
	first_process->p_s.reg_sp = ramtop;

	first_process->p_s.pc_epc = (memaddr)SSI_function_entry_point;
	
	// process tree to NULL already done by allocPcb()
	// p_time = 0 by allocPcb again, as well as p_supportStruct
	
	// (7)
	pcb_t *second_process = allocPcb();
	insertProcQ(&ready_queue, second_process);
	process_count++;
	// TODO: controllare come sopra
	second_process->p_s.status = STATUS_INTERRUPT_ON_NEXT;
	// FRAMESIZE should be ram's frames -> const PAGESIZE (4kB)
	second_process->p_s.reg_sp = ramtop - (2 * PAGESIZE);
	
	second_process->p_s.pc_epc = (memaddr)test;
	

	// (8)
	// Call the Scheduler
	schedule();
}
