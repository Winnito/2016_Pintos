#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

void swap_init(void)
{
	swap_block = block_get_role(BLOCK_SWAP);				
	
	if(swap_block == NULL) 											
		return;

	swap_map = bitmap_create(block_size(swap_block) / SECTORS_PER_PAGE); 

	if(swap_map == NULL) 
		return;

	bitmap_set_all(swap_map, 0);		
	lock_init(&swap_lock);					
}	

void swap_in(size_t used_index, void *kaddr)
{

	if(swap_block == NULL || swap_map == NULL)		
		return;

	lock_acquire(&swap_lock);											

	if(bitmap_test(swap_map, used_index) == 0)		
	{
		NOT_REACHED();
	}		
	else																				
	{
		unsigned int i;
		for(i = 0; i < SECTORS_PER_PAGE; i++)
			block_read(swap_block, used_index * SECTORS_PER_PAGE + i, (uint8_t*) kaddr + i * BLOCK_SECTOR_SIZE);

		bitmap_flip(swap_map, used_index);					
	}

	lock_release(&swap_lock);											
}

size_t swap_out(void *kaddr)
{
	if(swap_block == NULL || swap_map == NULL)		
		NOT_REACHED();

	lock_acquire(&swap_lock);											
	
	
	size_t free_index = bitmap_scan_and_flip(swap_map, 0, 1, 0);	

	if(free_index == BITMAP_ERROR)					
	{
		NOT_REACHED();
	}
	else																				
	{
		unsigned int i;
		for(i = 0; i < SECTORS_PER_PAGE; i++)
			block_write(swap_block, free_index * SECTORS_PER_PAGE + i, (uint8_t *) kaddr+ i * BLOCK_SECTOR_SIZE);
	}

	lock_release(&swap_lock);									

	return free_index;
}
