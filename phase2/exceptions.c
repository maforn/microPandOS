// TODO:  This module implements the TLB, Program Trap, and SYSCALL exception handlers. Furthermore, this module will contain the provided skeleton TLB-Refill event handler (e.g. uTLB_RefillHandler).
#include "./headers/exceptions.h"
#include "../phase1/headers/pcb.h"
#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

void uTLB_RefillHandler() {
	setENTRYHI(0x80000000);
	setENTRYLO(0x00000000);
	TLBWR();
	LDST((state_t*) 0x0FFFF000);
}

void exceptionHandler() {
	unsigned int cause = getCAUSE();
	// if the first bit is 1 it's an interrupt
	if (cause >= 1 << 31) {
		cause -= 1 << 31;
		if (cause == 3) { // interval timer
			// load the interval timer again
			LDIT(PSECOND);
			while ( !emptyProcQ( &blocked_pcbs[SEMDEVLEN][0] ) ) {
				// remove from waiting interval and put in ready
				insertProcQ(&ready_queue, removeProcQ(&blocked_pcbs[SEMDEVLEN][0]));
				soft_block_count--;
			}
			// TODO: controllare quale state va loaddato
			LDST((state_t *)BIOSDATAPAGE);
		} 
		else if (cause == 7) { // PLT timer
			// non abbiamo chiamato setTIMER perché viene fatto nello scheduel
			
			current_process->p_s = (state_t *)BIOSDATAPAGE;
			insertProcQ(&ready_queue, current_process);
			schedule();
		}
		else if (cause == 17) { // Disk Device

		}
		else if (cause == 18) { // Flash Device

		}
	}
	else {

	}
	/* Interrupt Exception code Description
	1 3 Interval Timer
	1 7 PLT Timer
	1 17 Disk Device
	1 18 Flash Device
	1 18 Ethernet Device
	1 20 Printer Device
	1 21 Termimal Device
	0 0-7, 11-24 Trap
	0 8-11 Syscall
	0 24-28 TLB exceptions */

}
