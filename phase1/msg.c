#include "./headers/msg.h"

static msg_t msgTable[MAXMESSAGES];
LIST_HEAD(msgFree_h);

void initMsgs() {
	for (int i = 0; i < MAXMESSAGES; i++)
		list_add_tail(&(msgTable[i].m_list), &msgFree_h);
}

void freeMsg(msg_t *m) {
	list_add_tail(&(m->m_list), &msgFree_h);
}

msg_t *allocMsg() {
	if (list_empty(&msgFree_h))
		return NULL;
	// get the pointer to the first msg after the head
	msg_t *msg = container_of(msgFree_h.next, msg_t, m_list);
	// remove the message from msgFree list
	list_del(&(msg->m_list));
	// reinitialize the vars
	INIT_LIST_HEAD(&(msg->m_list));
	msg->m_sender = NULL;
	msg->m_payload = 0;
	return msg;
}

void mkEmptyMessageQ(struct list_head *head) {
	INIT_LIST_HEAD(head);
}

int emptyMessageQ(struct list_head *head) {
	return list_empty(head);
}

void insertMessage(struct list_head *head, msg_t *m) {
	list_add_tail(&(m->m_list), head);	
}

void pushMessage(struct list_head *head, msg_t *m) {
}

msg_t *popMessage(struct list_head *head, pcb_t *p_ptr) {
}

msg_t *headMessage(struct list_head *head) {
}
