#ifndef SSI_H_INCLUDED
#define SSI_H_INCLUDED

#include "../../headers/types.h"

// TODO: check optimal solution
typedef struct ssi_answer_do_io {
	unsigned short status;
	unsigned short device;
	unsigned short controller;
} ssi_answer_do_io_t;

typedef union do_io_instruct {
	ssi_do_io_t request;
	ssi_answer_do_io_t answer;
} do_io_instruct_t;

void SSI_function_entry_point();

#endif
