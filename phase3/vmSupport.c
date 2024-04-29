#include "../headers/const.h"
#include "../headers/types.h"
#include "./headers/vmSupport.h"
#include "../phase2/headers/initial.h"
#include "./headers/vmSupport.h"
#include "./headers/initProc.h"
#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>

extern swap_t swap_table[POOLSIZE];
extern pcb_t *ssi_pcb, *swap_mutex_pcb;

unsigned int getFrameAddr(int frame_index){
    return SWAPSTARTADDR + frame_index * PAGESIZE;
}

void update_TLB(pteEntry_t pte){
	// TODO: switch to better approach
	TLBCLR();
	setENTRYHI(pte.pte_entryHI);
	setENTRYLO(pte.pte_entryLO);
	TLBWR();

	/* better approach
	setENTRYHI(pte.pte_entryHI);
	setENTRYLO(pte.pte_entryLO);
	TLBP(); // probe TLB

	if(getINDEX() == 0)
		TLBWI(); // if a match is found, rewrite it
	else
		TLBWR(); // otherwise, add new entry
	*/
}

support_t* getSupStruct(){
    support_t *sup_struct;
    ssi_payload_t getsup_payload = {
        .service_code = GETSUPPORTPTR,
        .arg = NULL,
    };
    SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&getsup_payload), 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&sup_struct), 0);

    return sup_struct;
}

unsigned int readWriteFlash(int operation, int page, int frame, int devnum){
    devreg_t *controller = (devreg_t *)DEV_REG_ADDR(IL_FLASH, devnum);
    controller->dtp.data0 = getFrameAddr(frame);

    // TODO: andrà bene p?
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

unsigned int readFromFlash(int page, int frame, int devnum){
    return readWriteFlash(FLASHREAD, page, frame, devnum);
}

unsigned int writeToFlash(int page, int frame, int devnum){
    return readWriteFlash(FLASHWRITE, page, frame, devnum);
}

int pickSwapFrame(){
    // TOOD: replace with better algorithm
    static int frame_index = 0;
    int i = frame_index;
    frame_index = (frame_index + 1) % POOLSIZE;
    return i;
}

void TLB_ExceptionHandler(){

    // (1) get support structure of current process
    support_t *sup_struct = getSupStruct();

    // (2) determine the cause of the exception
    state_t proc_state = sup_struct->sup_exceptState[PGFAULTEXCEPT];
    int cause = proc_state.cause;

    // (3) if it's a TLB Mod, pass to trap handler
    if(cause != TLBINVLDL && cause != TLBINVLDS){
        // TODO: pass to trap hadler
    }

    // (4) TODO: gain mutual exclusion
    SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_pcb, 0, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)swap_mutex_pcb, 0, 0);

    // (5) determine missing page number
    int p = proc_state.entry_hi & GETPAGENO;

    // (6) pick a frame from swap pool
    int i = pickSwapFrame();

    // (7 & 8) determine if frame i is occupied. If so, free the frame by copying the
    // data to the appropriate flash device and updating TLB
    if(swap_table[i].sw_asid != FREEFRAME){
        // disable interrupts to achieve atomicity
        unsigned int status = getSTATUS();
        setSTATUS(status & (~MSTATUS_MIE_MASK));

        // mark page as not valid and update TLB to reflect change
        swap_table[i].sw_pte->pte_entryLO &= (~VALIDON);
        update_TLB(*swap_table[i].sw_pte);

        // reenable interrupts
        setSTATUS(status);
        
        // save data to corresponding flash device
        int occupying_asid = swap_table[i].sw_asid;
        int occupying_page = swap_table[i].sw_pageNo;
        unsigned int io_status = writeToFlash(occupying_page, i, occupying_asid);

        if(io_status != 1){
            // TODO: pass to trap handler
        }
    }

    // (9) read page p from flash device into frame i
    unsigned int io_status = readFromFlash(p, i, sup_struct->sup_asid);
    // check status for errors
    if(io_status){
        // TODO: pass to trap handler
    }
    
    
    // (10) update swap table
    swap_table[i].sw_pageNo = p;
    swap_table[i].sw_asid = sup_struct->sup_asid;
    swap_table[i].sw_pte = &sup_struct->sup_privatePgTbl[p];

    // (11 & 12) update process page table and TLB atomically TODO: check

    // disable interrupts to achieve atomicity
    unsigned int status = getSTATUS();
    setSTATUS(status & (~MSTATUS_MIE_MASK));

    // update page table
    unsigned int elo = sup_struct->sup_privatePgTbl[p].pte_entryLO;
    sup_struct->sup_privatePgTbl[p].pte_entryLO = (i << PFNSHIFT) | VALIDON | (elo & DIRTYON & GLOBALON);

    // update TLB
    update_TLB(sup_struct->sup_privatePgTbl[p]);

    // reenable interrupts
    setSTATUS(status);

    // TODO: (13) release mutual exclusion
    SYSCALL(SENDMESSAGE, (unsigned int)swap_mutex_pcb, 0, 0);

    // (14) return control to current process
    LDST(&proc_state);
}