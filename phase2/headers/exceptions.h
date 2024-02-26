#ifndef EXCEPTIONS_H_INCLUDED
#define EXCEPTIONS_H_INCLUDED

#include "../../headers/types.h"

void uTLB_RefillHandler();
void exceptionHandler();

void sendMessage(state_t *proc_state);
void receiveMessage(state_t *proc_state);

#endif
