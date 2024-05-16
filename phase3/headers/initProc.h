#ifndef INITPROC_H_INCLUDED
#define INITPROC_H_INCLUDED

#include "../../headers/types.h"

#define QPAGE (PAGESIZE / 4)

pcb_t *create_process(state_t *s, support_t *sup);
extern pcb_t *swap_mutex_pcb;
extern pcb_t *initiator_pcb;

#endif
