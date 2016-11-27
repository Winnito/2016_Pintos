#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

struct list_elem* get_next_lru_clock(void)
{
	if(lru_clock == NULL)
		return NULL;

	if(list_size(&lru_list) == 1)
		return NULL;

	struct list_elem *next_elem = list_next(lru_clock);

	if(next_elem == list_end(&lru_list))
		next_elem = list_begin(&lru_list);

	return next_elem;
}

void* try_to_free_pages(enum palloc_flags flags)
{
	if(list_empty(&lru_list) == true)
	{
		lru_clock = NULL;
		return NULL;
	}

	if(lru_clock == NULL)
		lru_clock = list_begin(&lru_list);

	while(lru_clock)
	{

		struct list_elem *next = get_next_lru_clock();
		struct page *page = list_entry(lru_clock, struct page, lru);

		lru_clock = next;

		struct thread *t = page->thread;
		if(pagedir_is_accessed(t->pagedir, page->vme->vaddr))
			pagedir_set_accessed(t->pagedir, page->vme->vaddr, false);

		else
		{
			if(pagedir_is_dirty(t->pagedir, page->vme->vaddr) || page->vme->type == VM_ANON)
			{
				if(page->vme->type == VM_FILE)
				{
					lock_acquire(&filesys_lock);
					file_write_at(page->vme->file, page->kaddr, page->vme->read_bytes, page->vme->offset);
					lock_release(&filesys_lock);
				}					
				else if(page->vme->type != VM_ERROR)
				{
					page->vme->type = VM_ANON;
					page->vme->swap_slot = swap_out(page->kaddr);
				}
			}

			page->vme->is_loaded = false;
	    del_page_from_lru_list(page);

	    pagedir_clear_page(t->pagedir, page->vme->vaddr);
	    palloc_free_page(page->kaddr);
	    free(page);

	    return palloc_get_page(flags);
		}
	}
	return NULL;
}

void lru_list_init(void)
{
	list_init(&lru_list);
	lock_init(&lru_list_lock);
	lru_clock = NULL;
}

void add_page_to_lru_list(struct page *page)
{
	lock_acquire(&lru_list_lock);
	list_push_back(&lru_list, &page->lru);
	lock_release(&lru_list_lock);
}

void del_page_from_lru_list(struct page *page)
{
	list_remove(&page -> lru);
}

struct page* alloc_page(enum palloc_flags flags)
{
	void *kaddr = palloc_get_page(flags);

	while(kaddr == NULL)
	{
		lock_acquire(&lru_list_lock);
		kaddr = try_to_free_pages(flags);
		lock_release(&lru_list_lock);
	}
	struct page *page = (struct page*)malloc(sizeof(struct page));

	if(page == NULL)
		return NULL;
	
	page->kaddr = kaddr;
	page->thread = thread_current();
	page->vme = NULL;

	add_page_to_lru_list(page);

	return page;
}

void free_page(void *kaddr)
{
	struct list_elem *e;
	for(e = list_begin(&lru_list); e != list_end(&lru_list);)
	{
		struct list_elem *next_elem = list_next(e);
		struct page *page = list_entry(e, struct page, lru);
		if(page->kaddr == kaddr)
		{
			__free_page(page);
			break;
		}
		e = next_elem;
	}
}

void __free_page(struct page *page)
{
	lock_acquire(&lru_list_lock);
	del_page_from_lru_list(page);
	lock_release(&lru_list_lock);
	palloc_free_page(page->kaddr);
	free(page);
}

