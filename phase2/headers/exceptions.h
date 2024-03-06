#ifndef EXCEPTIONS_H_INCLUDED
#define EXCEPTIONS_H_INCLUDED

#define INTERRUPT_BIT 0x80000000
#define NOT_INTERRUPT_MASK 0x7FFFFFFF

#include "../../headers/types.h"

void uTLB_RefillHandler();
void exceptionHandler();

void sendMessage(state_t *proc_state);
void receiveMessage(state_t *proc_state);
void resumeExecution(state_t* proc_state);

#endif
