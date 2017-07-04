#include "vm/frame.h"
#include "userprog/syscall.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include <list.h>
#include "threads/vaddr.h"



static int cnt = 0;

void *
allocate_frame (struct spt_entry * entry, enum palloc_flags flag)
{
	void * frame;
	struct frame_entry * fe;
	// printf("<<allocate-s>>\n");
	// lock_acquire (&frame_lock);

	// user pool 이 아니기 때문에
	if ((flag != PAL_USER) && (flag != (PAL_ZERO|PAL_USER))) {
		return NULL;
	}
	// lock_acquire(&frame_lock);
	frame = palloc_get_page(flag);
	// lock_release(&frame_lock);

	if (frame != NULL) {
		// lock_acquire(&frame_lock);

		fe = (struct frame_entry *) malloc(sizeof(struct frame_entry));
		fe->frame = frame;
		fe->spte = entry;
		fe->thread = thread_current();

		lock_acquire (&frame_lock);
		list_push_back (&frame_table, &fe->frame_elem);
		lock_release (&frame_lock);

		// lock_release(&frame_lock);
	}

	// 만약 null 이라면 available page 가 없는 것이므로, evict 를 해야함
	else {
		// lock_acquire(&frame_lock);
		frame_evict(flag);
		// lock_release (&frame_lock);
		
		frame = palloc_get_page(flag);
		fe = (struct frame_entry *) malloc(sizeof(struct frame_entry));
		fe->frame = frame;
		fe->spte = entry;
		fe->thread = thread_current();
		lock_acquire (&frame_lock);
		list_push_back (&frame_table, &fe->frame_elem);
		lock_release (&frame_lock);

		 // lock_release (&frame_lock);
	}

	// printf ("end allocate: %d , ", list_size(&frame_table));

	// lock_release (&frame_lock);
	// printf("<<allocate-e>\n");
	return frame;
}


void
remove_frame (void * frame)
{
	struct list_elem *e;
	// printf("<<remove-s>>\n");
	lock_acquire(&frame_lock);

	// printf ("start remove: %d\n", list_size(&frame_table));


	for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) 
	{
		if (frame == list_entry(e, struct frame_entry, frame_elem)->frame) {
			pagedir_clear_page(list_entry(e, struct frame_entry, frame_elem)->thread->pagedir, list_entry(e, struct frame_entry, frame_elem)->spte->v_address);
			// lock_acquire (&frame_lock);
			list_remove(e);
			// lock_release (&frame_lock);
			free(list_entry(e, struct frame_entry, frame_elem));
			palloc_free_page(frame);
			break;
		}
	}
	lock_release(&frame_lock);
	// printf("<<remove-e>>\n");


	// printf ("end remove: %d\n", list_size(&frame_table));
	
	

}


void
frame_evict (enum palloc_flags flag) 
{
	struct list_elem * e;
	struct frame_entry * fe;
	void * free_frame;
	struct thread * fe_thread;
	bool not_found;
	// printf("<<eviect-s>>\n");
	lock_acquire (&frame_lock);

	// printf ("start evict: %d\n", list_size(&frame_table));

	e = list_begin (&frame_table);
	not_found = true;
	// printf("<<eviect-1>>\n");
	while (not_found) 
	{	
		// printf("<<eviect-2>>\n");
		fe = list_entry (e, struct frame_entry, frame_elem);
		fe_thread = fe->thread;
		// printf("<<eviect-3>>\n");
		if (list_size (&frame_table) == 0) {
			break;
		}
		// printf("<<eviect-4>>\n");
		if(is_kernel_vaddr(fe->spte->v_address)){
			// printf("<<eviect-4-000>>\n");
			e = list_next (e);
		}
		else if (pagedir_is_accessed (fe_thread->pagedir, fe->frame)) {
			pagedir_set_accessed (fe_thread->pagedir, fe->frame, false);
			// printf("<<eviect-4-0>>\n");
			e = list_next (e);
		}

		else {

			if (is_kernel_vaddr(fe->spte->v_address)) {
				e = list_next(e);
				// printf("<<eviect-4-1>>\n");
			}

			else { 
				fe->spte->type = 2;
				swap_out (fe->spte, fe->frame);
				pagedir_clear_page(fe_thread->pagedir, fe->spte->v_address);
				list_remove(e);
				palloc_free_page (fe->frame);
				free(fe);
				not_found = false;
				// printf("<<eviect-4-2>>\n");

			}
		}
		// printf("<<eviect-5>>\n");
		if (e == list_end(&frame_table) && (not_found == true)) {
			e = list_begin (&frame_table);
		}
		// printf("<<eviect-6>>\n");
	}
	lock_release (&frame_lock);
	// printf ("end evict: %d\n", list_size(&frame_table));
	// printf("<<eviect-e>>\n");
	// lock_release(&frame_lock2);
}



