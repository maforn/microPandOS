#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include "./headers/utils.h"
#include "./headers/interrupts.h"
#include "./headers/ssi.h"
#include "./headers/exceptions.h"
#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

extern pcb_t *ssi_pcb;

void loadOrSchedule() {
    if (current_process == NULL)
	    schedule();
    else
	    LDST((state_t *)BIOSDATAPAGE);
}

void handleIntervalTimer() {
    // load the interval timer again
    LDIT(PSECOND);
    while ( !emptyProcQ( &waiting_IT ) ) {
	// TODO: le variabili si sovrascrivono? arg è passato bene?
        // remove from waiting interval and send unlock to ssi
        ssi_payload_t payload = {
            .service_code = UNBLOCKPROCESSTIMER,
            .arg = removeProcQ(&waiting_IT)
        };
        state_t custom_state;
        custom_state.reg_a0 = SENDMESSAGE;
        custom_state.reg_a1 = (unsigned int)ssi_pcb;
        custom_state.reg_a2 = (unsigned int)&payload;
        sendMessage(&custom_state);
        // TODO: cosa facciamo se non riesce a mandare i messaggi a ssi_pcb?
        if (custom_state.reg_a0 == MSGNOGOOD) {}
    }
    loadOrSchedule();
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
    
    if (!emptyProcQ(&blocked_pcbs[device_number][controller_number])) {
        ssi_unblock_do_io_t ssi_unblock_process;
        ssi_unblock_process.status = status & STATUS_MASK;
        ssi_unblock_process.device = device_number;
        ssi_unblock_process.controller = controller_number;
        ssi_payload_t payload = {
            .service_code = UNBLOCKPROCESSDEVICE,
            .arg = &ssi_unblock_process,
        };
        state_t custom_state;
        custom_state.reg_a0 = SENDMESSAGE;
        custom_state.reg_a1 = (unsigned int)ssi_pcb;
        custom_state.reg_a2 = (unsigned int)&payload;
        sendMessage(&custom_state);
        // TODO: cosa facciamo se non riesce a mandare i messaggi a ssi_pcb?
        if (custom_state.reg_a0 == MSGNOGOOD) {}
    }
    loadOrSchedule();
}
