#ifndef SYSSUPPORT_H_INCLUDED
#define SYSSUPPORT_H_INCLUDED

#include "../../phase2/headers/initial.h"

#define PARENT 0
void generalExceptionHandler();
void programTrapExceptionHandler();
support_t *getSupStruct();

#endif
