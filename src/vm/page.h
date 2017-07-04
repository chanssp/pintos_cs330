#include <hash.h>
#include "filesys/file.h"


struct spt_entry
{
	void * v_address;
	struct hash_elem spt_elem;
	int type;	// 1 = file, 2 = swap, 4 = mmap
	bool valid;

	int read_bytes;
	int zero_bytes;
	int ofs;

	bool writable;
	struct file *file;

	int swap_disk_index;
};


bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
unsigned page_hash (const struct hash_elem *p_, void *aux);
struct spt_entry * page_lookup ( void *addr);

struct spt_entry * initialize_file_spte (struct spt_entry *spte, struct file * file, off_t ofs, 
								uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
bool insert_into_spt (struct spt_entry * entry);

void load_page (struct spt_entry * entry);

void swap_load (struct spt_entry * entry);
void mmap_load (struct spt_entry * entry);

void stack_growth (void *fault_addr);