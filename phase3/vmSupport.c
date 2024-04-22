#define SWAPSTARTADDR (RAMSTART + 32 * PAGESIZE))

typedef struct page_table_entry {
	memaddr VPN;		// Virtual Page Number: logical page number
	unsigned short ASID;	// U-proc’s unique ID
	short D;		// Dirty bit
	short V;		// Valid bit
	short G;		// Private for the specific ASID
} page_table_entry

page_table_entry page_table[32];
