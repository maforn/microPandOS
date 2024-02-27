#ifndef SSI_H_INCLUDED
#define SSI_H_INCLUDED

#include "../../headers/types.h"

#define UNBLOCKPROCESS 8

// TODO: check optimal solution
typedef struct ssi_unblock_do_io {
	unsigned short status;
	unsigned short device;
	unsigned short controller;
} ssi_unblock_do_io_t;

void SSI_function_entry_point();

void terminateProcess(pcb_t *arg);

#endif
