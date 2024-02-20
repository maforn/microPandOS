// TODO:  This module implements the TLB, Program Trap, and SYSCALL exception handlers. Furthermore, this module will contain the provided skeleton TLB-Refill event handler (e.g. uTLB_RefillHandler).
#include "./headers/exceptions.h"
#include "../phase1/headers/pcb.h"
#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

// controllare funzioni
void *memcpy(void *dst, const void *src, unsigned int len) {
	unsigned int i;
	if ((unsigned int)dst % sizeof(long) == 0 &&
             (unsigned int)src % sizeof(long) == 0 &&
             len % sizeof(long) == 0) {

                 long *d = dst;
                 const long *s = src;

                 for (i=0; i<len/sizeof(long); i++) {
                         d[i] = s[i];
                 }
         }
         else {
                 char *d = dst;
                 const char *s = src;

                 for (i=0; i<len; i++) {
                         d[i] = s[i];
                 }
         }

         return dst;
}

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
			
			// TODO: trovare qualcosa come memcpy
			memcpy(&current_process->p_s, (state_t *)BIOSDATAPAGE, sizeof(state_t));
			insertProcQ(&ready_queue, current_process);
			schedule();
		}
		else if (cause == 17) { // Disk Device

		}
		else if (cause == 18) { // Flash Device

		}
	}
	else {
		if (cause >= 8 && cause <= 11) { // Syscall
			state_t *proc_state  = (state_t *)BIOSDATAPAGE;
			if (!(proc_state->status & STATUS_MPP_ON)) { // user mode
				/*The above two Nucleus services are considered privileged services and are only available to processes
executing in kernel-mode. Any attempt to request one of these services while in user-mode should
trigger a Program Trap exception response.
6
In particular the Nucleus should simulate a Program Trap exception when a privileged service is
requested in user-mode. This is done by setting Cause.ExcCode in the stored exception state to RI
(Reserved Instruction), and calling one’s Program Trap exception handler.
Technical Point: As described above [Section 4], the saved exception state (for Processor 0) is
stored at the start of the BIOS Data Page (0x0FFF.F000) [Section 3.2.2-pops].*/
			}
			else {
				if (proc_state->reg_a0 == SENDMESSAGE) {
					
				}
				else if (proc_state->reg_a0 == RECEIVEMESSAGE) {

				}
			}
		}
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
