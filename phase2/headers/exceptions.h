#ifndef EXCEPTIONS_H_INCLUDED
#define EXCEPTIONS_H_INCLUDED

#define INTERRUPT_BIT (1U << 31)

#include "../../headers/types.h"

void uTLB_RefillHandler();
void exceptionHandler();

void SendMessage(state_t *proc_state);
void ReceiveMessage(state_t *proc_state);

#endif
