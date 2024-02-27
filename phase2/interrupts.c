#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include "./headers/utils.h"
#include "./headers/interrupts.h"
#include "./headers/ssi.h"
#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

extern pcb_t *ssi_pcb;

void handleIntervalTimer() {
    // load the interval timer again
    LDIT(PSECOND);
    while ( !emptyProcQ( &waiting_IT ) ) {
        // remove from waiting interval and put in ready
        insertProcQ(&ready_queue, removeProcQ(&waiting_IT));
        soft_block_count--;
    }
    if (current_process == NULL)
	    schedule();
    else
	    LDST((state_t *)BIOSDATAPAGE);
}

void handleLocalTimer() {
    // non abbiamo chiamato setTIMER perché viene fatto nello schedule
    memcpy(&current_process->p_s, (state_t *)BIOSDATAPAGE, sizeof(state_t));
    // add time passed to the process that has finished its timeslice
    // TODO: controllare senso di aggiungere arbitrariamente
    current_process->p_time += TIMESLICE;
    insertProcQ(&ready_queue, current_process);
    schedule();
}

void handleDeviceInterrupt(unsigned short device_number) {
	unsigned int interrupt_area = *(unsigned int *)CDEV_BITMAP_ADDR(device_number + DEV_IL_START);
    unsigned short controller_number = 7;
    // TODO: scegliere tra loop e costanti.
    for (int i = 0; i < 7; i++) {
        if (interrupt_area & (1 << i)) {
            controller_number = i;
            break;
        }
    }
    /*switch (interrupt_area & 0xff) {
        case DEV0ON:
            controller_number = 0;
            break;
        case DEV1ON:
            controller_number = 1;
            break;
        case DEV2ON:
            controller_number = 2;
            break;
        case DEV3ON:
            controller_number = 3;
            break;
        case DEV4ON:
            controller_number = 4;
            break;
        case DEV5ON:
            controller_number = 5;
            break;
        case DEV6ON:
            controller_number = 6;
            break;
        case DEV7ON:
            controller_number = 7;
            break;
    }*/
    devreg_t *controller = (devreg_t *)DEV_REG_ADDR(device_number + DEV_IL_START, controller_number);
    unsigned short status;
    if (device_number == IL_TERMINAL - DEV_IL_START) { // it's a terminal
        if ((controller->term.transm_status & STATUS_MASK) != 1 && (controller->term.transm_status & STATUS_MASK) != 3) { // channel is not busy or ready: it's the right one
            status = controller->term.transm_status;
            controller->term.transm_command = ACK;
        }
        else { // else it's the other channel
            status = controller->term.recv_status;
            controller->term.recv_command = ACK;
        }
    }
    else { // it's another type of device
        status = controller->dtp.status;
        controller->dtp.command = ACK;
    }
    // TODO: chiamare direttamente la funzione, non usare una SYSCALL da qui
    if (!emptyProcQ(&blocked_pcbs[device_number][controller_number])) {
        ssi_unblock_do_io_t ssi_unblock_process = {.status = status & STATUS_MASK, .device = device_number, .controller = controller_number};
        ssi_payload_t payload = {
            .service_code = UNBLOCKPROCESS,
            .arg = &ssi_unblock_process,
        }; 
        SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
    }
    if (current_process == NULL)
        schedule();
    else
        LDST((state_t*)BIOSDATAPAGE);
}
