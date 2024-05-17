#include "./headers/initial.h"
#include "./headers/scheduler.h"
#include "./headers/utils.h"
#include "./headers/interrupts.h"
#include "./headers/ssi.h"
#include "./headers/exceptions.h"
#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

/**
 * This is a support function that will load back the state of the process that received the interrupt
 * if it is not null, else it will call the schedule
*/
void loadOrSchedule() {
    if (current_process == NULL)
	    schedule();
    else
	    LDST((state_t *)BIOSDATAPAGE);
}

/**
 * This function is called by the exceptionHandler to handle the Interval Timer interrupt:
 * if there are some processes waiting (waiting_IT queue is not empty) then it sends a message to the
 * ssi process to unlock them
 */
void handleIntervalTimer() {
    // load the interval timer again
    LDIT(PSECOND);
    if ( !emptyProcQ( &waiting_IT ) ) {
        // send the unlock instruction to the ssi with a custom payload
        static ssi_payload_t payload = {
            .service_code = UNBLOCKPROCESSTIMER,
            .arg = NULL
        };

        // create a custom status for the sendMessage function
        static state_t custom_state;
        custom_state.reg_a0 = SENDMESSAGE;
        custom_state.reg_a1 = (unsigned int)ssi_pcb;
        custom_state.reg_a2 = (unsigned int)&payload;

        // send custom message: we cannot use SYSCALL as we are already in an exception
        sendMessage(&custom_state);
    }

    // return control to current process or call scheduler
    loadOrSchedule();
}


/**
 * This function is called by the exceptionHandler to handle the Local Timer (PLT) interrupt:
 * it will update the current process status and time passed and enqueue its pcb in
 * the ready queue. It will then call the scheduler to pass control to other processes.
 */
void handleLocalTimer() {
    if (current_process != NULL) {
        // update current process status
    	current_process->p_s = *(state_t *)BIOSDATAPAGE;

    	// add time passed to the process that has finished its timeslice
    	current_process->p_time += TIMESLICE;

        // re-insert process at the end of the ready queue
    	insertProcQ(&ready_queue, current_process);
    }

    // call schedule to pass control to first process in ready queue
    schedule();
}

static ssi_unblock_do_io_t ssi_unblock_process[DEVINTNUM][DEVPERINT];
static ssi_payload_t payloads[DEVINTNUM][DEVPERINT];

/**
 * This function is called by the exceptionHandler to handle a Device interrupt: it will check the
 * interrupt device bit area based on the device number, send an ACK to the controller and send a 
 * message to the ssi with the status of the operation
 * @param device_number the device number as a number between 0 and 4 (disk, flash, ethernet, printer, terminal)
 */
void handleDeviceInterrupt(unsigned short device_number) {
    // get the interrupt bit with a macro based on the interrupt line (DEV_IL_START = 17)
	unsigned int interrupt_area = *(unsigned int *)CDEV_BITMAP_ADDR(device_number + DEV_IL_START);
    // iterate on the bits to find the controller number which sent the interrupt
    unsigned short controller_number = 7;
    for (int i = 0; i < 7; i++) {
        if (interrupt_area & (1 << i)) {
            controller_number = i;
            break;
        }
    }

    // get the controller as a device (a union between a terminal and a normal device)
    devreg_t *controller = (devreg_t *)DEV_REG_ADDR(device_number + DEV_IL_START, controller_number);
    unsigned short status;

    // switch union based on type, then save the status and send the ACK to the controller
    if (device_number == IL_TERMINAL - DEV_IL_START) { // it's a terminal
        // the terminal has two command and status: select the one that is not busy or installed (thus 
        // has finished or encountered an error)
        if ((controller->term.transm_status & STATUS_MASK) != 1 && (controller->term.transm_status & STATUS_MASK) != 3) { // channel is not busy or ready: it's the right one
            status = controller->term.transm_status;
            controller->term.transm_command = ACK;
        }
        else { // else it's the other channel
            status = controller->term.recv_status;
            controller->term.recv_command = ACK;
        }
    }
    else { // it's another type of device, not a terminal
        status = controller->dtp.status;
        controller->dtp.command = ACK;
    }
    
    // if the program that has requested the IO operation is still alive, send a message to the SSI to unlock it
    if (!emptyProcQ(&blocked_pcbs[device_number][controller_number])) {
        // save the relevant parameters as a payload
        ssi_unblock_process[device_number][controller_number].status = status & STATUS_MASK;
        ssi_unblock_process[device_number][controller_number].device = device_number;
        ssi_unblock_process[device_number][controller_number].controller = controller_number;

        // build the ssi payload
        payloads[device_number][controller_number].service_code = UNBLOCKPROCESSDEVICE;
        payloads[device_number][controller_number].arg = &ssi_unblock_process[device_number][controller_number];

        // create a custom state for a fake sendMessage
        state_t custom_state;
        custom_state.reg_a0 = SENDMESSAGE;
        custom_state.reg_a1 = (unsigned int)ssi_pcb;
        custom_state.reg_a2 = (unsigned int)&payloads[device_number][controller_number];

        // manually send the message as we cannot use SYSCALL inside an exception
        sendMessage(&custom_state);
    }

    // return control to current process or call scheduler
    loadOrSchedule();
}
