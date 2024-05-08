#ifndef VMSUPPORT_H_INCLUDED
#define VMSUPPORT_H_INCLUDED

#include "../../headers/types.h"

#define SWAPSTARTADDR (RAMSTART + 32 * PAGESIZE)
#define FREEFRAME -1
#define PFNSHIFT 12
#define TLBMOD 24

extern swap_t swap_table[POOLSIZE]; // swap table
void TLB_ExceptionHandler();

void initSwapStructs();

#endif
