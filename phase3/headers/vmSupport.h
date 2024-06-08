#ifndef VMSUPPORT_H_INCLUDED
#define VMSUPPORT_H_INCLUDED

#include "../../headers/types.h"

#define SWAPSTARTADDR (RAMSTART + 32 * PAGESIZE)
#define FREEFRAME -1
#define INDEX_MASK 0x80000000
#define PFNSHIFT 12
#define TLBMOD 24

extern swap_t swap_table[POOLSIZE]; // swap table

// handles TLB exceptions
void TLB_ExceptionHandler();

// initializes the swap table
void initSwapStructs();

// frees all the frames associated to the specified asid
void freeProcFrames(int asid);

#endif
