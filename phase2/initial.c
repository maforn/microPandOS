#include "../phase1/headers/msg.h"
#include "./headers/initial.h"
#include <uriscv/liburiscv.h>

// (1)
// In the header file
int process_count, soft_block_count;
struct list_head ready_queue;
pcb_t *current_process;
struct list_head blocked_pcbs[DEVINTNUM][DEVPERINT];
struct list_head waiting_IT;
struct list_head waiting_MSG;
pcb_t *ssi_pcb;
// TODO: Blocked PCBs controllare la correttezza

#include "./headers/scheduler.h"
#include "./headers/exceptions.h"
#include "./headers/ssi.h"

//extern void test();
void test() {
	pcb_PTR test_pcb = current_process;

	unsigned int payload = 15;
	unsigned int dst = 0;

    // test send and receive
	SYSCALL(SENDMESSAGE, (unsigned int)test_pcb, (memaddr)&payload, 0);
    pcb_PTR sender = (pcb_PTR)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (memaddr)&dst, 0);

		if (sender != test_pcb)
        	PANIC();
		if (dst != (memaddr)&payload)
			PANIC();

	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (memaddr)5, 0);
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
	mkEmptyProcQ(&waiting_MSG);

	// (5)
	LDIT(PSECOND);

	// (6)
	ssi_pcb = allocPcb();
	insertProcQ(&ready_queue, ssi_pcb);
	process_count++;
	ssi_pcb->p_s.mie = MIE_ALL;

	// TODO: ricontrollare che sia giusto
	ssi_pcb->p_s.status = STATUS_INTERRUPT_ON_NEXT;
	// Obtain ramtop with the macro
	memaddr ramtop;
	RAMTOP(ramtop);
	ssi_pcb->p_s.reg_sp = ramtop;

	ssi_pcb->p_s.pc_epc = (memaddr)SSI_function_entry_point;
	
	// process tree to NULL already done by allocPcb()
	// p_time = 0 by allocPcb again, as well as p_supportStruct
	
	// (7)
	pcb_t *second_process = allocPcb();
	insertProcQ(&ready_queue, second_process);
	process_count++;
	second_process->p_s.mie = MIE_ALL;
	// TODO: controllare come sopra
	second_process->p_s.status = STATUS_INTERRUPT_ON_NEXT;
	// FRAMESIZE should be ram's frames -> const PAGESIZE (4kB)
	second_process->p_s.reg_sp = ramtop - (2 * PAGESIZE);
	
	second_process->p_s.pc_epc = (memaddr)test;
	

	// (8)
	// Call the Scheduler
	schedule();
}
