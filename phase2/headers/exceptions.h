#ifndef EXCEPTIONS_H_INCLUDED
#define EXCEPTIONS_H_INCLUDED

#define INTERRUPT_BIT (1U << 31)

void uTLB_RefillHandler();
void exceptionHandler();

#endif
