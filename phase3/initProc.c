#include "../headers/types.h"
#include "../phase2/headers/initial.h"
#include <uriscv/liburiscv.h>
#include "./headers/initProc.h"

pcb_t * swap_mutex_pcb;
swap_t swap_table[POOLSIZE];
