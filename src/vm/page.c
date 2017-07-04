#include "threads/thread.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include <string.h>
#include "userprog/pagedir.h"
#include "userprog/syscall.h"



bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
 const struct spt_entry *a = hash_entry (a_, struct spt_entry, spt_elem);
 const struct spt_entry *b = hash_entry (b_, struct spt_entry, spt_elem);
 
 return a->v_address < b->v_address;
}


unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) 
{
 const struct spt_entry *p = hash_entry (p_, struct spt_entry, spt_elem);
 
 return hash_bytes (&p->v_address, sizeof p->v_address);
}


struct spt_entry *
page_lookup (void * addr)
{
	struct spt_entry p;
	struct hash_elem *e;
	struct thread * cur = thread_current();

	p.v_address = addr;
	e = hash_find(&cur->supp_page_table, &p.spt_elem);

	return e != NULL ? hash_entry (e, struct spt_entry, spt_elem) : NULL;
}


struct spt_entry *
initialize_file_spte (struct spt_entry * entry, struct file * file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
	ASSERT (entry != NULL);

	entry->writable = writable;
	entry->zero_bytes = zero_bytes;
	entry->read_bytes = read_bytes;
	entry->v_address = upage;
	entry->ofs = ofs;
	entry->file = file;
	entry->type = 1;		// file type
	entry->valid = false;

	return entry;
}

struct spt_entry *
initialize_mmap_spte (struct spt_entry * entry, struct file * file, off_t ofs, uint8_t *upage,
				uint32_t read_bytes, uint32_t zero_bytes) {
	ASSERT (entry != NULL);

	entry->file = file;
	entry->ofs = ofs;
	entry->read_bytes = read_bytes;
	entry->zero_bytes = zero_bytes;
	entry->v_address = upage;
	entry->type = 4;	// mmap type
	entry->valid = false;

	return entry;
}


bool
insert_into_spt (struct spt_entry * entry)
{
	struct hash_elem * e;
	struct thread * cur = thread_current();

	e = hash_insert (&cur->supp_page_table, &entry->spt_elem);
	if (e != NULL)	{
		return false;
	}

	return true;
}


void
load_page (struct spt_entry * entry)
{
	/* file type */
	
	
	/* swap type */
	swap_load (entry);
	
	/* mmap type (will implement in project 3-2) */
	/*
	else if (entry->type == 4)
	{
		
	}
	*/
}




void
swap_load (struct spt_entry * entry)
{
	void * frame;
	bool success;
	struct thread * cur = thread_current();

	frame = allocate_frame(entry, PAL_USER);
	ASSERT (frame != NULL);

	success = pagedir_set_page(cur->pagedir, entry->v_address, frame, entry->writable);

	if (!success) {
		remove_frame(frame);
		PANIC ("<SWAP LOAD FAIL>");
	}

	swap_in (entry, frame);
}


void
stack_growth (void *fault_addr)
{
	void * frame;
	bool success;
	struct thread * cur = thread_current();
	struct spt_entry * spte;

	spte = (struct spt_entry *) malloc (sizeof(struct spt_entry));

	frame = allocate_frame(spte, PAL_USER);
	ASSERT (frame != NULL);

	success =	pagedir_set_page(cur->pagedir, fault_addr, frame, true);	//fault address not needed

	if (!success) {
		remove_frame(frame);
		PANIC ("<STACK GROWTH FAILED>");
	}

	spte->v_address=fault_addr;
	insert_into_spt(spte);

}






