#ifndef INITIAL_H_INCLUDED
#define INITIAL_H_INCLUDED

extern int process_count, soft_block_count;
extern struct list_head ready_queue;
extern pcb_t *current_process;
extern struct list_head blocked_pcbs[SEMDEVLEN][2];

// set MPIE to 1 and MPP to 11
#define STATUS_INTERRUPT_ON_NEXT (3 << 11) + (1 << 7) 
// set MIE to 1 and MPP to 11
#define STATUS_INTERRUPT_ON_NOW (3 << 11) + (1 << 3) 

#endif
