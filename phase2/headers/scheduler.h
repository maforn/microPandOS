#ifndef SCHEDULER_H_INCLUDED
#define SCHEDULER_H_INCLUDED

#define MIE_ALL_BUT_PLT (MIE_ALL & ~MIE_MTIE_MASK)

void schedule();

#endif
