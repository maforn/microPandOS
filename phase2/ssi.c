#include "./headers/ssi.h"
#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include "./headers/utils.h"
#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

int createProcess(pcb_t * sender, ssi_create_process_t *arg);

void terminateProcess(pcb_t *arg);

void doIO(pcb_t *sender, ssi_do_io_t *do_io);

void unblockProcessDevice(ssi_unblock_do_io_t *do_io);

void SSIRequest(pcb_t* sender, int service, void* arg);

void SSI_function_entry_point() {
	while (1) {
		char msg = 'a';
		char *s = &msg;
		unsigned int *base = (unsigned int*)(0x10000254);
		unsigned int *command = base + 3;

		unsigned int value = PRINTCHR | (((unsigned int)*s) << 8);
		ssi_do_io_t do_io = {
			.commandAddr = command,
			.commandValue = value,
		};
		debug(&do_io);
		doIO(current_process, &(do_io));
		while(1) {}

		ssi_payload_t payload;
		pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);
		SSIRequest(sender, payload.service_code, &payload.arg);
	}
}

int createProcess(pcb_t * sender, ssi_create_process_t *arg) {
	pcb_t *new_pcb = allocPcb();
	if (new_pcb == NULL) // no new proc allocable
		return NOPROC;
	memcpy(&(new_pcb->p_s), arg->state, sizeof(state_t));
	memcpy(new_pcb->p_supportStruct, arg->support, sizeof(support_t));
	insertChild(sender, new_pcb);
	insertProcQ(&ready_queue, new_pcb);
	process_count++;
	return (int)new_pcb;
}

void terminateProcess(pcb_t *arg) {
	while (!emptyProcQ(&arg->p_child))
		terminateProcess(headProcQ(&arg->p_child));
	outChild(arg);
	process_count--;
	if (current_process == arg)
		current_process = NULL;
	else if (outProcQ(&ready_queue, arg) != NULL) {
		soft_block_count--;
		// TODO: controllare lo tiri fuori
		list_del(&arg->p_list);
	}
	freePcb(arg);
}

void doIO(pcb_t *sender, ssi_do_io_t *do_io) {
	// block the pcb: get the device and controller from the address
	unsigned short device_number = ((unsigned int)do_io->commandAddr - START_DEVREG) / (DEVPERINT  * DEVREGSIZE);
	unsigned short controller_number = (((unsigned int)do_io->commandAddr - START_DEVREG) / DEVREGSIZE) % DEVPERINT;
	// save the new current status
	memcpy(&(sender->p_s), (state_t *)BIOSDATAPAGE, sizeof(state_t));
	insertProcQ(&blocked_pcbs[device_number][controller_number], sender);
	// write on device address specified
	*(do_io->commandAddr) = do_io->commandValue;
}

void unblockProcessFromDevice(ssi_unblock_do_io_t *do_io) {
	SYSCALL(SENDMESSAGE, (unsigned int)removeProcQ(&blocked_pcbs[do_io->device][do_io->controller]), (unsigned int)do_io->status, 0);
}

void SSIRequest(pcb_t* sender, int service, void* arg) {
	switch (service) {
		case CREATEPROCESS:
			SYSCALL(SENDMESSAGE, (unsigned int)sender, (unsigned int)createProcess(sender, (ssi_create_process_t *)arg), 0);
			break;
		case TERMPROCESS:
			if (arg == NULL)
				arg = sender;
			terminateProcess(arg);
			if (arg != sender)
				SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
			break;
		case DOIO:
			doIO(sender, arg);
			break;
		case GETTIME:

			break;
		case CLOCKWAIT:

			break;
		case GETSUPPORTPTR:

			break;
		case GETPROCESSID:

			break;
		case UNBLOCKPROCESS:
			unblockProcessFromDevice(arg);
			break;
		default: // terminate process and all of its children
			terminateProcess(sender);
	}
}
