#include "./headers/pcb.h"

static pcb_t pcbTable[MAXPROC];  /* PCB array with maximum size 'MAXPROC' */
LIST_HEAD(pcbFree_h);            /* List of free PCBs                     */
static int next_pid = 1;

void initPcbs() {
    for (int i=0;i<MAXPROC;i++){
        pcb_t *p = &pcbTable[i];
        list_add(&p->p_list,&pcbFree_h);
    }
}

void freePcb(pcb_t *p) {
    list_add(&p->p_list,&pcbFree_h);
}

pcb_t *allocPcb() {
    if (list_empty(&pcbFree_h))
        return NULL;
    pcb_t *p = container_of(pcbFree_h.next,pcb_t,p_list);
    list_del(&p->p_list);
    p->p_parent = NULL;
    INIT_LIST_HEAD(&(p->p_child));
    INIT_LIST_HEAD(&(p->msg_inbox));
    p->p_pid=next_pid;
    next_pid++;
    p->p_s.cause=0;
    p->p_s.entry_hi=0;
    for (int i = 0; i < 32; i++)
        p->p_s.gpr[i]=0;
    p->p_s.mie=0;
    p->p_s.pc_epc=0;
    p->p_s.status=0;
    INIT_LIST_HEAD(&(p->p_sib));
    p->p_supportStruct=NULL;
    p->p_time=0;
    return p;
}

void mkEmptyProcQ(struct list_head *head) {
    INIT_LIST_HEAD(head);
}

int emptyProcQ(struct list_head *head) {
    return list_empty(head);
}

void insertProcQ(struct list_head* head, pcb_t* p) {
    list_add_tail(&(p->p_list), head);
}

pcb_t* headProcQ(struct list_head* head) {
    if (emptyProcQ(head))
        return NULL;
    return container_of(head->next,pcb_t,p_list);
}

pcb_t* removeProcQ(struct list_head* head) {
    if (emptyProcQ(head))
        return NULL;
    pcb_t *p = container_of(head->next,pcb_t,p_list);
    list_del(head->next);
    return p;
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
    if (emptyProcQ(head))
        return NULL;
    pcb_t *iter;
    list_for_each_entry(iter,head,p_list){
        if (iter==p){
            list_del(&(p->p_list));
            return p;
        }
    }
    return NULL;
}

int emptyChild(pcb_t *p) {
	return list_empty(&(p->p_child));
}

void insertChild(pcb_t *prnt, pcb_t *p) {
	// 1. set parent of *p to *prnt
	p->p_parent = prnt;

	// 2. add *p to *prnt's children list
	list_add_tail(&(p->p_sib), &(prnt->p_child));
}

pcb_t* removeChild(pcb_t *p) {
	if (list_empty(&(p->p_child))) return NULL;

	// remove *p's first child
	pcb_t* removed_child = container_of(p->p_child.next, pcb_t, p_child);
	list_del(p->p_child.next);

	return removed_child;
}

pcb_t* outChild(pcb_t *p) {
	if (p->p_parent == NULL) return NULL;

	// remove p from parent's children list
	list_del(&(p->p_sib));

	// set *p's parent to NULL
	p->p_parent = NULL;
	return p;
}
