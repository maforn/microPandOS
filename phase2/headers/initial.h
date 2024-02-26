#ifndef INITIAL_H_INCLUDED
#define INITIAL_H_INCLUDED

#include "../../phase1/headers/pcb.h"

extern int process_count, soft_block_count;
extern struct list_head ready_queue;
extern pcb_t *current_process;
extern struct list_head blocked_pcbs[DEVINTNUM][DEVPERINT];
extern struct list_head waiting_IT;
extern pcb_t *ssi_pcb;

#define STATUS_MPP_ON (3 << 11)
#define STATUS_MPIE_ON (1 << 7)
#define STATUS_MIE_ON (1 << 3)
// set MPIE to 1 and MPP to 11
#define STATUS_INTERRUPT_ON_NEXT (STATUS_MPP_ON + STATUS_MPIE_ON) 
// set MIE to 1 and MPP to 11
#define STATUS_INTERRUPT_ON_NOW (STATUS_MPP_ON + STATUS_MIE_ON) 

#endif
