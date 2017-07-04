#include <bitmap.h>
#include "devices/disk.h"
#include "vm/page.h"
#include "threads/synch.h"

/*--map holding disk index---*/
struct bitmap * disk_map;

/*--the swap disk--*/
struct disk * swap_disk;

struct lock swap_lock;

void swap_init (void);
void swap_out (struct spt_entry * entry, void * frame);
void swap_in (struct spt_entry * entry, void * frame);