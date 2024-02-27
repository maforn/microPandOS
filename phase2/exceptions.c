#include "./headers/exceptions.h"
#include "./headers/initial.h"
#include "./headers/interrupts.h"
#include "./headers/scheduler.h"
#include "./headers/utils.h"
#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>
#include <uriscv/types.h>
#include "../phase1/headers/msg.h"

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
			// TODO: find RI
			setCAUSE(2);
			exceptionHandler();
			// TODO: find RI
			setCAUSE(2);
			exceptionHandler();
			}
			else {
				// TODO: is the PC update necessary in every case? 
				proc_state->pc_epc += 4;

				if (proc_state->reg_a0 == SENDMESSAGE)
					sendMessage(proc_state);
				else if (proc_state->reg_a0 == RECEIVEMESSAGE)
					receiveMessage(proc_state);
				else{
					// Pass up
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

void sendMessage(state_t *proc_state){
	pcb_t *dst = (pcb_t *)proc_state->reg_a1;

	// dst doesn't exist
	if(isFree(&dst->p_list)){
		proc_state->reg_a0 = DEST_NOT_EXIST; //failed
	}
	// dst is waiting for a message from current process
	else if(contains(&waiting_MSG, &dst->p_list)
			&& ((dst->p_s).reg_a1 == ANYMESSAGE || (pcb_t*)(dst->p_s).reg_a1 == current_process)){

		// copy message and sender in designated memory areas
		if ((void *)(dst->p_s).reg_a2 != NULL)
			memcpy((memaddr*)(dst->p_s).reg_a2, &(proc_state->reg_a2), sizeof(memaddr));
		(dst->p_s).reg_a0 = (memaddr)current_process;
			
		// awake receveing process and update count
		insertProcQ(&ready_queue, dst); 
		soft_block_count--;

		proc_state->reg_a0 = 0; // success
	}
	else{ // dst is in ready queue or (waiting for I/O or timer) or waiting for message from different sender
		msg_t *msg = allocMsg();

		if(msg == NULL){ // no available messages
			proc_state->reg_a0 = MSGNOGOOD; // failed
		}
		else{
			// add message to dst inbox
			msg->m_payload = proc_state->reg_a2; 
			msg->m_sender = current_process;

			pushMessage(&dst->msg_inbox, msg);
			proc_state->reg_a0 = 0; // success
		}
	}

	// update current process state and resume execution
	memcpy(&current_process->p_s, proc_state, sizeof(state_t));
	LDST(&current_process->p_s);
}

void receiveMessage(state_t *proc_state){
	pcb_t *sender = (pcb_t *)proc_state->reg_a1;
	msg_t *msg = popMessage(&current_process->msg_inbox, sender);

	if (msg == NULL){
		// TODO: save processor state and update CPU time
	 	memcpy(&current_process->p_s, proc_state, sizeof(state_t));

		// block process
		insertProcQ(&waiting_MSG, current_process);
		schedule();
	}
	else{
		// transfer data
		if((void *)proc_state->reg_a2 != NULL)
			memcpy((memaddr*)proc_state->reg_a2, &(msg->m_payload), sizeof(memaddr));
		proc_state->reg_a0 = (memaddr)msg->m_sender;
		freeMsg(msg);

		// update current process state and resume execution
		memcpy(&current_process->p_s, proc_state, sizeof(state_t));
		LDST(&current_process->p_s);
	}
}
void SendMessage(state_t *proc_state){
	pcb_t *dst = (pcb_t *)proc_state->reg_a1;

	// dst doesn't exist
	if(isFree(&dst->p_list)){
		proc_state->reg_a0 = DEST_NOT_EXIST; //failed
	}
	// dst is waiting for a message from current process
	else if(contains(&blocked_pcbs[SEMDEVLEN][1], &dst->p_list)
			&& ((dst->p_s).reg_a1 == ANYMESSAGE || (pcb_t*)(dst->p_s).reg_a1 == current_process)){

		// copy message and sender in designated memory areas
		memcpy(&(dst->p_s).reg_a2, &proc_state->reg_a2, sizeof(unsigned int));
		memcpy(&(dst->p_s).reg_a0, &current_process->p_pid, sizeof(int));
			
		// awake receveing process and update count
		insertProcQ(&ready_queue, dst); 
		soft_block_count--;

		proc_state->reg_a0 = 0; // success
	}
	else{ // dst is in ready queue or (waiting for I/O or timer) or waiting for message from different sender
		msg_t *msg = allocMsg();

		if(msg == NULL){ // no available messages
			proc_state->reg_a0 = MSGNOGOOD; // failed
		}
		else{
			// add message to dst inbox
			msg->m_payload = proc_state->reg_a2; 
			msg->m_sender = current_process;

			pushMessage(&dst->msg_inbox, msg);
			proc_state->reg_a0 = 0; // success
		}
	}

	// update current process state and resume execution
	memcpy(&current_process->p_s, proc_state, sizeof(state_t));
	LDST(&current_process->p_s);
}

void ReceiveMessage(state_t *proc_state){
	pcb_t *sender = (pcb_t *)proc_state->reg_a1;
	msg_t *msg = popMessage(&current_process->msg_inbox, sender);

	if (msg == NULL){
		// TODO: save processor state and update CPU time
	 	memcpy(&current_process->p_s, proc_state, sizeof(state_t));

		// block process
		insertProcQ(&blocked_pcbs[SEMDEVLEN][1], current_process);
		schedule();
	}
	else{
		// transfer data
		proc_state->reg_a0 = msg->m_sender->p_pid;
		memcpy(&proc_state->reg_a2, &msg->m_payload, sizeof(unsigned int));
		freeMsg(msg);

		// update current process state and resume execution
		memcpy(&current_process->p_s, proc_state, sizeof(state_t));
		LDST(&current_process->p_s);
	}
}