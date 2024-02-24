// TODO: This module implements the device/timer interrupt exception handler. This module will process all the device/timer interrupts, converting device/timer interrupts into appropriate messages for the blocked PCBs
#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include "./headers/utils.h"
#include <uriscv/liburiscv.h>

void handleIntervalTimer() {
    // load the interval timer again
    LDIT(PSECOND);
    while ( !emptyProcQ( &waiting_IT ) ) {
        // remove from waiting interval and put in ready
        insertProcQ(&ready_queue, removeProcQ(&waiting_IT));
        soft_block_count--;
    }
    // TODO: controllare quale state va loaddato
    LDST((state_t *)BIOSDATAPAGE);
}

void handlePLT() {
    // non abbiamo chiamato setTIMER perché viene fatto nello schedule
	
    // TODO: trovare qualcosa come memcpy
    memcpy(&current_process->p_s, (state_t *)BIOSDATAPAGE, sizeof(state_t));
    // add time passed to the process that has finished its timeslice
    // TODO: controllare senso di aggiungere arbitrariamente
    current_process->p_time += TIMESLICE;
    insertProcQ(&ready_queue, current_process);
    schedule();
}
