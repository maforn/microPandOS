#include "../phase1/headers/msg.h"
#include "./headers/initial.h"
#include <uriscv/liburiscv.h>

// (1)
// support counting vars
int process_count, soft_block_count;

// queue of process that are ready to run 
struct list_head ready_queue;

// process executing at the moment
pcb_t *current_process;

// the three lists for blocked pcbs
struct list_head blocked_pcbs[DEVINTNUM][DEVPERINT]; // processes waiting IO operation
struct list_head waiting_IT; // processes waiting Interval Timer tick
struct list_head waiting_MSG; // processes waiting a message

// pcb of the System Service Interface and its alias
pcb_t *true_ssi_pcb;
// set a memory address that is not used for anything else, as it is in kseg0 and thus used for 
// BIOS routines, not memory
pcb_t *ssi_pcb = (pcb_t*)42;

#include "./headers/scheduler.h"
#include "./headers/exceptions.h"
#include "./headers/ssi.h"

extern void test();

int main() {
	// ATT: per le librerie di uriscv guardare /urs/local/include/uriscv
	// (2) set up the pass_up_vector
	memaddr *passup_addr = (memaddr *)PASSUPVECTOR;
	struct passupvector *pass_up_vector = (struct passupvector *)passup_addr;
	pass_up_vector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
	pass_up_vector->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
	pass_up_vector->exception_handler = (memaddr)exceptionHandler;
	pass_up_vector->exception_stackPtr = (memaddr)KERNELSTACK;
	
	// (3) init process and message spaces
	initPcbs();
	initMsgs();

	// (4) set up the initial variables
	process_count = 0;
	soft_block_count = 0;

	mkEmptyProcQ(&ready_queue);
	for (int i = 0; i < DEVINTNUM; i++) {
		for (int e = 0; e < DEVPERINT; e++)
			mkEmptyProcQ(&blocked_pcbs[i][e]);
	}
	mkEmptyProcQ(&waiting_IT);
	mkEmptyProcQ(&waiting_MSG);

	// (5) Load the global interval timer
	LDIT(PSECOND);

	// (6) allocate first pcb: the SSI
	true_ssi_pcb = allocPcb();
	insertProcQ(&ready_queue, true_ssi_pcb);
	process_count++;
	// set all the interrupts on
	true_ssi_pcb->p_s.mie = MIE_ALL;
	true_ssi_pcb->p_s.status = STATUS_INTERRUPT_ON_NEXT;
	// Obtain ramtop with the macro
	memaddr ramtop;
	RAMTOP(ramtop);
	// set stack pointer and pc
	true_ssi_pcb->p_s.reg_sp = ramtop;
	true_ssi_pcb->p_s.pc_epc = (memaddr)SSI_function_entry_point;
	
	// process tree to NULL already done by allocPcb()
	// p_time = 0 by allocPcb again, as well as p_supportStruct
	
	// (7) allocate the second pcb: the test process
	pcb_t *second_process = allocPcb();
	insertProcQ(&ready_queue, second_process);
	process_count++;
	second_process->p_s.mie = MIE_ALL;
	second_process->p_s.status = STATUS_INTERRUPT_ON_NEXT;
	// FRAMESIZE should be ram's frames -> const PAGESIZE (4kB)
	second_process->p_s.reg_sp = ramtop - (2 * PAGESIZE);
	
	second_process->p_s.pc_epc = (memaddr)test;
	

	// (8)
	// Call the Scheduler to start execution
	schedule();
}
