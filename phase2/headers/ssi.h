#ifndef SSI_H_INCLUDED
#define SSI_H_INCLUDED

#include "../../headers/types.h"

#define UNBLOCKPROCESSDEVICE 8
#define UNBLOCKPROCESSTIMER 9

// TODO: check optimal solution
typedef struct ssi_unblock_do_io {
	pcb_t *process;
	unsigned int status;
} ssi_unblock_do_io_t;

void SSI_function_entry_point();

void terminateProcess(pcb_t *arg);

#endif
