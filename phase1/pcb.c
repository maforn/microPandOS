#include "./headers/pcb.h"

static pcb_t pcbTable[MAXPROC];  /* PCB array with maximum size 'MAXPROC' */
LIST_HEAD(pcbFree_h);            /* List of free PCBs                     */
static int next_pid = 1;

void initPcbs() {
}

void freePcb(pcb_t *p) {
}

pcb_t *allocPcb() {
}

void mkEmptyProcQ(struct list_head *head) {
}

int emptyProcQ(struct list_head *head) {
}

void insertProcQ(struct list_head* head, pcb_t* p) {
}

pcb_t* headProcQ(struct list_head* head) {
}

pcb_t* removeProcQ(struct list_head* head) {
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
}

int emptyChild(pcb_t *p) {
	return list_empty(&(p->p_child));
}

void insertChild(pcb_t *prnt, pcb_t *p) {
	// 1. set parent of *p to *prnt
	p->p_parent = prnt;

	// 2. add *p to *prnt's children list
	list_add_tail(&(p->p_list), &(prnt->p_child));

	// 3. set *p's siblings list to the rest of *prnt's children
	LIST_HEAD(sib_list);
	struct list_head *iterPointer = prnt->p_child.next;
	while (iterPointer != &(prnt->p_child)) {
		if (container_of(iterPointer, pcb_t, p_child) != p)
			list_add(iterPointer, &sib_list);
		iterPointer = iterPointer->next;
	}
	p->p_sib = sib_list;
}

pcb_t* removeChild(pcb_t *p) {
	if (list_empty(&(p->p_child))) return NULL;

	// remove *p's first child
	pcb_t* removed_child = container_of(p->p_child.next, pcb_t, p_child);
	__list_del(&(p->p_child), p->p_child.next->next);

	return removed_child;
}

pcb_t* outChild(pcb_t *p) {
	if (p->p_parent == NULL) return NULL;

	// remove p from parent's children list
	struct list_head *iterPointer = p->p_parent->p_child.next;
	while (container_of(iterPointer, pcb_t, p_child) != p)
		iterPointer = iterPointer->next;
	__list_del(iterPointer->prev, iterPointer->next);

	// set *p's parent to NULL
	p->p_parent = NULL;

	return p;
}
