#include "./headers/exceptions.h"
#include "../phase1/headers/msg.h"
#include "./headers/initial.h"
#include "./headers/interrupts.h"
#include "./headers/scheduler.h"
#include "./headers/ssi.h"
#include "./headers/utils.h"
#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

static inline pteEntry_t getPteEntry(pcb_t *process, int i) {
  return process->p_supportStruct->sup_privatePgTbl[i];
}

void uTLB_RefillHandler() {
  state_t *proc_state = (state_t *)BIOSDATAPAGE;
  // get page number
  int p = (proc_state->entry_hi & GETPAGENO) >> VPNSHIFT;
  // set the new entry HI end LO, write to the TLB and Load back the process
  setENTRYHI(getPteEntry(current_process, p).pte_entryHI);
  setENTRYLO(getPteEntry(current_process, p).pte_entryLO);
  TLBWR();
  LDST(proc_state);
}

/**
 * This function will handle all the exceptions that are not implemented in
 * phase 2: traps, illegal syscalls and TLB exceptions. If the process that
 * generated the exception has not got a support struct it will be terminated,
 * else the exception will be passed up
 * @param excType is the type of exception that is thrown, it can be either
 * PGFAULTEXCEPT or GENERALEXCEPT
 */
void passUpOrDie(int excType) {
  if (current_process->p_supportStruct == NULL) {
    // terminate process and its progeny
    terminateProcess(current_process);
    // current process was terminated, call the scheduler
    schedule();
  } else { // pass up
    // copy the pass up state
    state_t *proc_state = (state_t *)BIOSDATAPAGE;
    support_t *support_struct = current_process->p_supportStruct;
    support_struct->sup_exceptState[excType] = *proc_state;
    // load the context that will handle this
    LDCXT(support_struct->sup_exceptContext[excType].stackPtr,
          support_struct->sup_exceptContext[excType].status,
          support_struct->sup_exceptContext[excType].pc);
  }
}

/**
 * This function is called by the kernel when an exception is thrown
 */
void exceptionHandler() {
  // get cause with macro
  unsigned int cause = getCAUSE();
  // if the first bit is 1 it's an interrupt
  if (cause & INTERRUPT_BIT) {
    // remove the interrupt bit so we can get the cause without the additional
    // bit
    cause &= NOT_INTERRUPT_MASK;
    if (cause == IL_TIMER) // Global Interval Timer
      handleIntervalTimer();
    else if (cause == IL_CPUTIMER) // PLT timer
      handleLocalTimer();
    else if (cause == IL_DISK)
      handleDeviceInterrupt(IL_DISK - DEV_IL_START);
    else if (cause == IL_FLASH)
      handleDeviceInterrupt(IL_FLASH - DEV_IL_START);
    else if (cause == IL_ETHERNET)
      handleDeviceInterrupt(IL_ETHERNET - DEV_IL_START);
    else if (cause == IL_PRINTER)
      handleDeviceInterrupt(IL_PRINTER - DEV_IL_START);
    else if (cause == IL_TERMINAL)
      handleDeviceInterrupt(IL_TERMINAL - DEV_IL_START);
  }
  // not an interrupt
  else {
    // Trap
    if ((cause >= 0 && cause <= 7) || (cause > 11 && cause < 24)) {
      passUpOrDie(GENERALEXCEPT);
    } else if (cause >= 8 && cause <= 11) { // Syscall
      state_t *proc_state = (state_t *)BIOSDATAPAGE;
      // check if the process is in user mode
      if (!(proc_state->status & MSTATUS_MPP_MASK)) {
        // call the passup function
        passUpOrDie(GENERALEXCEPT);
      } else {
        // we need to update the program counter of the caller, or it will keep
        // calling the SYSCALL function
        proc_state->pc_epc += 4;

        // switch between types of SYSCALL
        if (proc_state->reg_a0 == SENDMESSAGE) {
          sendMessage(proc_state);
          resumeExecution(proc_state);
        } else if (proc_state->reg_a0 == RECEIVEMESSAGE)
          receiveMessage(proc_state);
        // Syscalls not directly handled by the nucleus
        else
          passUpOrDie(GENERALEXCEPT);
      }
    }
    // TLB exceptions: call pass up
    else if (cause >= 24 && cause <= 28) {
      passUpOrDie(PGFAULTEXCEPT);
    }
  }
}

/**
 * This function implements an asynchronous send.
 * It may also be called manually, so it does not contain calls to the scheduler
 * or similar. SYCALL(a0, a1, a2, a3) will save the parameters in reg_a0,
 * reg_a1, reg_a2 and reg_a3 (gpr[24-27])
 * @param proc_state state of the process when interrupted, it contains the
 * paramters with which the SYSCALL was called
 */
void sendMessage(state_t *proc_state) {
  // get the destination from the second parameter of the SYSCALL
  pcb_t *dst = (pcb_t *)proc_state->reg_a1;
  if (dst == ssi_pcb)
    dst = true_ssi_pcb;

  // dst doesn't exist: fails
  if (isFree(&dst->p_list)) {
    proc_state->reg_a0 = DEST_NOT_EXIST;
  }
  // dst is waiting for a message and the message is either from the current
  // process or from anyone
  else if (contains(&waiting_MSG, &dst->p_list) &&
           ((dst->p_s).reg_a1 == ANYMESSAGE ||
            (pcb_t *)(dst->p_s).reg_a1 == current_process)) {

    // copy message payload and sender in designated memory areas
    if ((void *)(dst->p_s).reg_a2 != NULL)
      memcpy((memaddr *)(dst->p_s).reg_a2, &(proc_state->reg_a2),
             sizeof(memaddr));

    // aliasing for ssi_pcb
    if (current_process == true_ssi_pcb)
      (dst->p_s).reg_a0 = (memaddr)ssi_pcb;
    else
      (dst->p_s).reg_a0 = (memaddr)current_process;

    // awake receveing process
    outProcQ(&waiting_MSG, dst);
    dst->blocked = 0;
    insertProcQ(&ready_queue, dst);

    proc_state->reg_a0 = 0; // success
  }
  // dst is in ready queue or (waiting for I/O or timer) or waiting for message
  // from different sender
  else {
    // try to alloc message
    msg_t *msg = allocMsg();

    if (msg == NULL) {                // no available space for messages
      proc_state->reg_a0 = MSGNOGOOD; // failed
    } else {
      // add message to dst inbox
      msg->m_payload = proc_state->reg_a2;
      msg->m_sender = current_process;

      insertMessage(&dst->msg_inbox, msg);
      proc_state->reg_a0 = 0; // success
    }
  }
}

/**
 * Asynchronous receive.
 * @param proc_state state of the process when interrupted, it contains the
 * paramters with which the SYSCALL was called
 */
void receiveMessage(state_t *proc_state) {
  pcb_t *sender = (pcb_t *)proc_state->reg_a1;
  if (sender == ssi_pcb)
    sender = true_ssi_pcb;

  // try to get a message from the inbox
  msg_t *msg = popMessage(&current_process->msg_inbox, sender);

  // if no messages are found block the process
  if (msg == NULL) {
    // update the time passed during the process' timeslice and save the current
    // state
    current_process->p_time += (TIMESLICE - getTIMER());
    current_process->p_s = *proc_state;

    // aliasing for ssi_pcb: if it's waiting for a message from ssi_pcb change
    // it to the true address
    if ((pcb_t *)current_process->p_s.reg_a1 == ssi_pcb)
      current_process->p_s.reg_a1 = (memaddr)true_ssi_pcb;

    // block process if it is not already blocked by the ssi in waitForClock or
    // doIO
    if (current_process->blocked == 0) {
      insertProcQ(&waiting_MSG, current_process);
      current_process->blocked = 1;
    }
    current_process = NULL;
    schedule();
  } else {
    // a message was found, transfer data
    if ((void *)proc_state->reg_a2 != NULL)
      memcpy((memaddr *)proc_state->reg_a2, &(msg->m_payload), sizeof(memaddr));

    // aliasing for the ssi_pcb
    if (msg->m_sender == true_ssi_pcb)
      proc_state->reg_a0 = (memaddr)ssi_pcb;
    else
      proc_state->reg_a0 = (memaddr)msg->m_sender;

    // free message space
    freeMsg(msg);

    resumeExecution(proc_state);
  }
}

/**
 * Helper function to resume the execution of the current process with the new
 * state proc_state
 */
void resumeExecution(state_t *proc_state) {
  // update current process state and resume execution
  current_process->p_s = *proc_state;
  LDST(&current_process->p_s);
}
