#ifndef INITIAL_H_INCLUDED
#define INITIAL_H_INCLUDED

#include "../../phase1/headers/pcb.h"

extern int process_count, soft_block_count;
extern struct list_head ready_queue;
extern pcb_t *current_process;
extern struct list_head blocked_pcbs[DEVINTNUM][DEVPERINT];
extern struct list_head waiting_IT;
extern struct list_head waiting_MSG;
extern pcb_t *true_ssi_pcb;
extern pcb_t *ssi_pcb;

// set MPIE to 1 and MPP to 11
#define STATUS_INTERRUPT_ON_NEXT (MSTATUS_MPP_M + MSTATUS_MPIE_MASK) 
// set MIE to 1 and MPP to 11
#define STATUS_INTERRUPT_ON_NOW (MSTATUS_MPP_M + MSTATUS_MIE_MASK) 

#endif
