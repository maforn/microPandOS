#include "../headers/types.h"
#include "../phase2/headers/initial.h"
#include "../headers/const.h"
#include "../testers/h/tconst.h" //TODO: sarebbe meglio non ci fosse ma mi saerve per PARENT, altrimenti ci metto 0 hardcoded

void generalExceptionHandler(){

    // get support struct
    support_t *support_struct = current_process->p_supportStruct;

    // get process state
    state_t* exceptionState = &(support_struct->sup_exceptState[GENERALEXCEPT]);

    // get the exception cause
    int cause = (exceptionState->cause&GETEXECCODE)>>CAUSESHIFT;

    //TODO: li distinguo cosi?
    if (cause == SYSEXCEPTION){
        // call the syscall exception handler
        SYSCALLExceptionHandler(exceptionState);
    } else if (cause == IOINTERRUPTS){
        // call the trap exception handler
        programTrapExceptionHandler(exceptionState);
    } else {
        //TODO: altrimenti muori?
        // terminate process and its progeny
        terminateProcess(current_process);
        // current process was terminated, call the scheduler
        schedule();
    }

}

void SYSCALLExceptionHandler(state_t* proc_state){
    if(proc_state->reg_a0 == SENDMSG){

        // get the destination process
        pcb_t *destination = (pcb_t *)proc_state->reg_a1;

        if(destination == PARENT){
            // send the message to the parent
            destination = current_process->p_parent;
        }

        // get the payload
        unsigned int payload = proc_state->reg_a2;

        // syscall to send the message
        SYSCALL(SENDMSG, (unsigned int)destination, (unsigned int)payload, 0);
        

    }else if(proc_state->reg_a0 == RECEIVEMSG){

        // get the sender process
        pcb_t *sender = (pcb_t *)proc_state->reg_a1;
        
        unsigned int *payload = (unsigned int *)current_process->p_s.reg_a0; //TODO: sicuramente sbagliato

        // syscall to receive the message
        SYSCALL(RECEIVEMSG, (unsigned int)sender, (unsigned int)payload, 0);

    }else{
        //TODO: altrimenti muori? anche qui? una strage di processi
        // terminate process and its progeny
        terminateProcess(current_process);
        // current process was terminated, call the scheduler
        schedule();
    }
}

void programTrapExceptionHandler(state_t* proc_state){

    ssi_payload_t payload;
    payload.service_code = TERMPROCESS;
    payload.arg = NULL;
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, &payload, 0);

    //TODO: se sta aspettando la mutua esclusione bisogna rilasciarla
}
