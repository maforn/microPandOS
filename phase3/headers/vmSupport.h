#ifndef VMSUPPORT_H_INCLUDED
#define VMSUPPORT_H_INCLUDED

#define SWAPSTARTADDR (RAMSTART + 32 * PAGESIZE)
#define FREEFRAME -1
#define PFNSHIFT 12

extern swap_t swap_table[POOLSIZE];     // swap table

void initSwapStructs();

#endif
