#ifndef SSI_H_INCLUDED
#define SSI_H_INCLUDED

#include "../../headers/listx.h"

// returns 1 if elem is in the list whose sentinel is head. Returns 0 otherwise
int contains(struct list_head head, struct list_head elem);

// implements C memcpy
void *memcpy(void *dst, const void *src, unsigned int len);
void debug(void *arg);

#endif