#include <list.h>
#include "threads/palloc.h"


struct frame_entry
{
	void * frame;
	struct list_elem frame_elem;
	struct spt_entry * spte;	// evict 할 때 필요.
	struct thread * thread;
};

struct list frame_table;		// frame table of user virtual memory
struct lock frame_lock;
struct lock frame_lock3;

void * allocate_frame (struct spt_entry * entry, enum palloc_flags flag);
void remove_frame (void * frame);

void frame_evict (enum palloc_flags flag);
