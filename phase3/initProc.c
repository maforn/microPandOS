#include "../headers/types.h"
#include "../phase2/headers/initial.h"

pcb_PTR test_pcb, swap_mutex, print0_pcb, print1_pcb;

/* a procedure to print on terminal 0
void print()
{
    while (1)
    {
        char *msg;
        unsigned int sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, (unsigned
int)(&msg), 0); char *s = msg; devregtr *base = (devregtr *)(TERM0ADDR);
        devregtr *command = base + 3;
        devregtr status;

        while (*s != EOS)
        {
            devregtr value = PRINTCHR | (((devregtr)*s) << 8);
            ssi_do_io_t do_io = {
                .commandAddr = command,
                .commandValue = value,
            };
            ssi_payload_t payload = {
                .service_code = DOIO,
                .arg = &do_io,
            };
            SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned
int)(&payload), 0); SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned
int)(&status), 0);

            if ((status & TERMSTATMASK) != RECVD)
                PANIC();

            s++;
        }
        SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
    }
}

void print_term0(char *s)
{
    SYSCALL(SENDMESSAGE, (unsigned int)print_pcb, (unsigned int)s, 0);
    SYSCALL(RECEIVEMESSAGE, (unsigned int)print_pcb, 0, 0);
} */

void test() { test_pcb = current_process; }
