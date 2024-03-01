#include "./headers/ssi.h"
#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include "./headers/utils.h"
#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

void SSIRequest(pcb_t* sender, int service, void* arg);

void SSI_function_entry_point() {
	while (1) {
		ssi_payload_t *payload;
		pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);
		SSIRequest(sender, payload->service_code, payload->arg);
	}
}

unsigned int createProcess(pcb_t * sender, ssi_create_process_t *arg) {
	pcb_t *new_pcb = allocPcb();
	if (new_pcb == NULL) // no new proc allocable
		return NOPROC;
	memcpy(&(new_pcb->p_s), arg->state, sizeof(state_t));
	if (arg->support == NULL)
		new_pcb->p_supportStruct = NULL;
	else
		memcpy(new_pcb->p_supportStruct, arg->support, sizeof(support_t));
	insertChild(sender, new_pcb);
	insertProcQ(&ready_queue, new_pcb);
	process_count++;
	return (unsigned int)new_pcb;
}

void terminateProcess(pcb_t *arg) {
	while (!emptyProcQ(&arg->p_child))
		terminateProcess(headProcQ(&arg->p_child));
	outChild(arg);
	process_count--;
	if (current_process == arg)
		current_process = NULL;
	else if (outProcQ(&ready_queue, arg) == NULL) {
		soft_block_count--;
		// TODO: controllare lo tiri fuori
		list_del(&arg->p_list);
	}
	freePcb(arg);
}

static inline void removeFromMessageQueue(pcb_t *process) {
	outProcQ(&waiting_MSG, process);
}

void doIO(pcb_t *sender, ssi_do_io_t *do_io) {
	removeFromMessageQueue(sender);
	// block the pcb: get the device and controller from the address
	unsigned short device_number = ((unsigned int)do_io->commandAddr - START_DEVREG) / (DEVPERINT  * DEVREGSIZE);
	unsigned short controller_number = (((unsigned int)do_io->commandAddr - START_DEVREG) / DEVREGSIZE) % DEVPERINT;
	// save the new current status
	sender->blocked = 1;
	insertProcQ(&blocked_pcbs[device_number][controller_number], sender);
	// write on device address specified
	*(do_io->commandAddr) = do_io->commandValue;
}

static inline void unblockProcessFromDevice(ssi_unblock_do_io_t *do_io) {
	pcb_t *proc = removeProcQ(&blocked_pcbs[do_io->device][do_io->controller]);
	insertProcQ(&waiting_MSG, proc);
	SYSCALL(SENDMESSAGE, (unsigned int)proc, (unsigned int)do_io->status, 0);
}

static inline void unblockProcessFromTimer() {
	while ( !emptyProcQ( &waiting_IT ) ) {
		pcb_t* process = removeProcQ(&waiting_IT);
		insertProcQ(&waiting_MSG, process);
		SYSCALL(SENDMESSAGE, (unsigned int)process, 0, 0);
	}
}

static inline void getCPUTime(pcb_t* sender){
    	SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)sender->p_time, 0);
}

static inline void waitForClock(pcb_t* sender){
   	//TODO: il processo che manda una waitForClock si aspetta un massaggio di risposta, quindi finirà nella coda dei processi che aspettano una risposta, quando verrà sbloccato dell'intervaltimer starà ancora aspettando di ricevere una risposta
	removeFromMessageQueue(sender);
	sender->blocked = 1;
    	insertProcQ(&waiting_IT, sender);
}

static inline void getSupportData(pcb_t* sender){
    	SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)sender->p_supportStruct, 0);
}

static inline void getProcessID(pcb_t*sender){
   	SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)sender->p_pid, 0);
}

void SSIRequest(pcb_t* sender, int service, void* arg) {
	switch (service) {
		case CREATEPROCESS:
			pcb_t *new_proc = (pcb_t *)createProcess(sender, (ssi_create_process_t *)arg);
			SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)new_proc, 0);
			break;
		case TERMPROCESS:
			if (arg == NULL)
				arg = sender;
			terminateProcess(arg);
			if (arg != sender)
				SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
			break;
		case DOIO:
			doIO(sender, (ssi_do_io_t*)arg);
			break;
		case GETTIME:
           		getCPUTime(sender);
			break;
		case CLOCKWAIT:
            		waitForClock(sender);
			break;
		case GETSUPPORTPTR:
            		getSupportData(sender);
			break;
		case GETPROCESSID:
	     	        getProcessID(sender);
			break;
		case UNBLOCKPROCESSDEVICE:
			unblockProcessFromDevice((ssi_unblock_do_io_t*)arg);
			break;
		case UNBLOCKPROCESSTIMER:
			unblockProcessFromTimer();
			break;
		default: // terminate process and all of its children
			terminateProcess(sender);
	}
}
