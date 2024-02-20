// TODO: This module implements the System Service Interface processo.
#include "./headers/ssi.h"
#include "./headers/initial.h"
#include "../headers/types.h"
#include <uriscv/liburiscv.h>

void *memcpy1(void *dst, const void *src, unsigned int len) {
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

int createProcess(pcb_t * sender, ssi_create_process_t *arg) {
	pcb_t *new_pcb = allocPcb();
	if (new_pcb == NULL) // no new proc allocable
		return NOPROC;
	memcpy1(&(new_pcb->p_s), arg->state, sizeof(state_t));
	memcpy1(new_pcb->p_supportStruct, arg->support, sizeof(support_t));
	insertChild(sender, new_pcb);
	insertProcQ(&ready_queue, new_pcb);
	process_count++;
	return (int)new_pcb;
}

void terminateProcessAndProgeny(pcb_t *arg) {
	while (!emptyProcQ(&arg->p_child))
		terminateProcessAndProgeny(headProcQ(&arg->p_child));
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

void terminateProcess(pcb_t *arg) {
	terminateProcessAndProgeny(arg);
	// TODO: controllare correttezza NON TORNA UNA CIPPA 
	/*if (current_process == NULL)
		schedule();*/
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

			break;
		case GETTIME:

			break;
		case CLOCKWAIT:

			break;
		case GETSUPPORTPTR:

			break;
		case GETPROCESSID:

			break;
		default: // terminate process and all of its children
			terminateProcess(sender);
	}
}

void SSI_function_entry_point() {
	while (1) {
		ssi_payload_t payload;
		pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned int)&payload, 0);
		SSIRequest(sender, payload.service_code, &payload.arg);
	}
}
