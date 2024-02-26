#include "./headers/exceptions.h"
#include "./headers/initial.h"
#include "./headers/interrupts.h"
#include "./headers/scheduler.h"
#include "./headers/utils.h"
#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

void uTLB_RefillHandler() {
	setENTRYHI(0x80000000);
	setENTRYLO(0x00000000);
	TLBWR();
	LDST((state_t*) 0x0FFFF000);
}

void exceptionHandler() {
	unsigned int cause = getCAUSE();
	// if the first bit is 1 it's an interrupt
	if (cause & INTERRUPT_BIT) {
		cause &= !INTERRUPT_BIT; // remove the interrupt bit so we can get the cause without the additional bit
		if (cause == IL_TIMER)  // interval timer
			handleIntervalTimer();
		else if (cause == IL_CPUTIMER)  // PLT timer
			handleLocalTimer();
		else if (cause == IL_DISK) // Disk Device
			handleDeviceInterrupt(IL_DISK - DEV_IL_START);
		else if (cause == IL_FLASH) // Flash Device
			handleDeviceInterrupt(IL_FLASH - DEV_IL_START);
		else if (cause == IL_ETHERNET)
			handleDeviceInterrupt(IL_ETHERNET - DEV_IL_START);
		else if (cause == IL_PRINTER) 
			handleDeviceInterrupt(IL_PRINTER - DEV_IL_START);
		else if (cause == IL_TERMINAL) 
			handleDeviceInterrupt(IL_TERMINAL - DEV_IL_START);
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
