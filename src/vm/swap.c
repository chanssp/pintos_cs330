#include "vm/swap.h"


void 
swap_init (void)
{
	int size, i;
	swap_disk = disk_get(1,1);

	size = 1<<10;	// size 2^10
	disk_map = bitmap_create(size);

	 lock_init (&swap_lock);

	for (i = 0; i < size; i++) {
 	   bitmap_set (disk_map, i, 0);
	}	
}


void
swap_out (struct spt_entry * entry, void * frame)
{
	uint32_t index;
	int i, sector_number;

	 lock_acquire (&swap_lock);

	index = bitmap_scan (disk_map, 0, 1, 0);

	sector_number = index * 8;

	for (i = 0; i < 8; i++)
		disk_write (swap_disk, sector_number + i, frame + i * DISK_SECTOR_SIZE);

	// 이 disk space 는 사용 중
	bitmap_set (disk_map, index, 1);

	entry->swap_disk_index = index;
	entry->valid = false;		// 여기서 해주는 것이 맞는지 나중에 확인.

	lock_release (&swap_lock);

}


void
swap_in (struct spt_entry * entry, void * frame)
{
	uint32_t index;
	int i, sector_number;

	lock_acquire (&swap_lock);

	index = entry->swap_disk_index;
	sector_number = index * 8;

	for (i = 0; i < 8; i++)
		disk_read (swap_disk, sector_number + i, frame + i * DISK_SECTOR_SIZE);

	// 다시 쓸 수 있는 disk space 가 생김
	bitmap_set (disk_map, index, 0);
	
	entry->valid = true;		// 여기서 해주는 것이 맞는지 나중에 확인.

	 lock_release(&swap_lock);
}
