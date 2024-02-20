// TODO: This module implements the System Service Interface processo.
#include "./headers/ssi.h"
#include "./headers/initial.h"
#include "../headers/types.h"
#include <uriscv/liburiscv.h>

void SSI_function_entry_point() {
	while (1) {
    		ssi_payload_t payload = {
    		    .service_code = GETPROCESSID,
    		    .arg = (void *)1,
    		};
		SYSCALL(SENDMESSAGE, (unsigned int)current_process, (unsigned int)(&payload), 0);
	}
}
