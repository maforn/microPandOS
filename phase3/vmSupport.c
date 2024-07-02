#include "./headers/vmSupport.h"
#include "../headers/const.h"
#include "../phase2/headers/initial.h"
#include "./headers/initProc.h"
#include "./headers/sysSupport.h"
#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>

swap_t swap_table[POOLSIZE]; // swap table
extern pcb_t *ssi_pcb, *swap_mutex_pcb;

void initSwapStructs() {
  // initialize all frames to be free. Since all asids are non-negative
  // integers, a negative value as asid can be used to mark a frame as free.
  for (int i = 0; i < POOLSIZE; i++)
    swap_table[i].sw_asid = FREEFRAME;
}

// gets physical frame address
static inline unsigned int getFrameAddr(int frame_index) {
  return SWAPSTARTADDR + frame_index * PAGESIZE;
}

// updates the TLB to include the specified entry
void update_TLB(pteEntry_t pte) {
  setENTRYHI(pte.pte_entryHI);
  setENTRYLO(pte.pte_entryLO);

  TLBP(); // probe TLB

  if ((getINDEX() & INDEX_MASK) == 0)
    TLBWI(); // if a match is found, rewrite it
  else
    TLBWR(); // otherwise, add new entry
}

// function to either:
// (1) read a flash device's page to a memory frame
// (2) write a memory frame to a flash device's page
unsigned int readWriteFlash(int operation, int page, int frame,
                            int controller_num) {
  static devreg_t *controller;
  controller = (devreg_t *)DEV_REG_ADDR(IL_FLASH, controller_num);
  controller->dtp.data0 = getFrameAddr(frame);

  ssi_do_io_t do_io = {
      .commandAddr = &controller->dtp.command,
      .commandValue = (page << 8) | operation,
  };
  ssi_payload_t payload = {
      .service_code = DOIO,
      .arg = &do_io,
  };
  unsigned int status;

  SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload), 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status), 0);

  return status;
}

// wrapper for readWriteFlash used for read operations
static inline unsigned int readFromFlash(int page, int frame,
                                         int controller_num) {
  return readWriteFlash(FLASHREAD, page, frame, controller_num);
}

// wrapper for readWriteFlash used for write operations
static inline unsigned int writeToFlash(int page, int frame,
                                        int controller_num) {
  return readWriteFlash(FLASHWRITE, page, frame, controller_num);
}

// picks a frame for a page, also implementing replacement policy
int pickSwapFrame() {
  // static var that is conserved
  static int frame_index = 0;
  int swap_frame = -1;

  // check if an unoccupied frame is present
  for (int i = 0; i < POOLSIZE && swap_frame == -1; i++) {
    if (swap_table[i].sw_asid == FREEFRAME)
      swap_frame = i;
  }

  // if no free frame was found, pick one based on "FIFO" policy
  if (swap_frame == -1)
    swap_frame = frame_index;

  // update frame_index for "FIFO" replacement
  if (swap_frame == frame_index)
    frame_index = (frame_index + 1) % POOLSIZE;

  return swap_frame;
}

// handles TLB exceptions
void TLB_ExceptionHandler() {

  // (1) get support structure of current process
  support_t *sup_struct = getSupStruct();

  // (2) determine the cause of the exception
  state_t proc_state = sup_struct->sup_exceptState[PGFAULTEXCEPT];
  int cause = proc_state.cause;

  // (3) if it's a TLB Mod, pass to trap handler
  if (cause == TLBMOD) {
    // pass to trap hadler
    programTrapExceptionHandler();
  }

  // (4) gain mutual exclusion
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_pcb, 0, 0);
  SYSCALL(RECEIVEMESSAGE, (unsigned int)swap_mutex_pcb, 0, 0);

  // (5) determine missing page number
  int p = (proc_state.entry_hi & GETPAGENO) >> VPNSHIFT;
  // the last possibile page does not follow the pattern: add a custom check
  if (p > USERPGTBLSIZE)
    p = USERPGTBLSIZE - 1;

  // (6) pick a frame from swap pool
  int i = pickSwapFrame();

  // (7 & 8) determine if frame i is occupied. If so, free the frame by copying
  // the data to the appropriate flash device and updating TLB
  if (swap_table[i].sw_asid != FREEFRAME) {
    // disable interrupts to achieve atomicity
    unsigned int status = getSTATUS();
    setSTATUS(status & (~MSTATUS_MIE_MASK));

    // mark page as not valid and update TLB to reflect change
    swap_table[i].sw_pte->pte_entryLO &= (~VALIDON);
    update_TLB(*(swap_table[i].sw_pte));

    // reenable interrupts
    setSTATUS(status);

    // save data to corresponding flash device
    int occupying_asid = swap_table[i].sw_asid;
    int occupying_page = swap_table[i].sw_pageNo;
    unsigned int io_status =
        writeToFlash(occupying_page, i, occupying_asid - 1);

    if (io_status != 1) {
      // pass to trap handler
      programTrapExceptionHandler();
    }
  }

  // (9) read page p from flash device into frame i
  unsigned int io_status = readFromFlash(p, i, sup_struct->sup_asid - 1);
  // check status for errors
  if (io_status != 1) {
    // pass to trap handler
    programTrapExceptionHandler();
  }

  // (10) update swap table
  swap_table[i].sw_pageNo = p;
  swap_table[i].sw_asid = sup_struct->sup_asid;
  swap_table[i].sw_pte = &sup_struct->sup_privatePgTbl[p];

  // (11 & 12) update process page table and TLB atomically

  // disable interrupts to achieve atomicity
  unsigned int status = getSTATUS();
  setSTATUS(status & (~MSTATUS_MIE_MASK));

  // update page table
  unsigned int elo = sup_struct->sup_privatePgTbl[p].pte_entryLO;
  sup_struct->sup_privatePgTbl[p].pte_entryLO =
      getFrameAddr(i) | VALIDON | (elo & (DIRTYON | GLOBALON));

  // update TLB
  update_TLB(sup_struct->sup_privatePgTbl[p]);

  // reenable interrupts
  setSTATUS(status);

  // (13) release mutual exclusion
  SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_pcb, 0, 0);

  // (14) return control to current process
  LDST(&proc_state);
}

// frees all the proc frames by setting them to FREEFRAME (-1)
void freeProcFrames(int asid) {
  for (int i = 0; i < POOLSIZE; i++) {
    if (swap_table[i].sw_asid == asid)
      swap_table[i].sw_asid = FREEFRAME;
  }
}
