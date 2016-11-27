#include <string.h>
#include <stdbool.h>
#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/interrupt.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

static unsigned vm_hash_func(const struct hash_elem *e, void *aux);
static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b); 
void vm_destroy_func(struct hash_elem *e, void *aux);



void vm_init(struct hash *vm) 											
{
	hash_init(vm,vm_hash_func,vm_less_func,NULL);
}

static unsigned vm_hash_func(const struct hash_elem *e, void *aux) 		
{
	return hash_int((int)(hash_entry(e,struct vm_entry,elem)->vaddr));

}

static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b) 	
{
	return hash_entry(a,struct vm_entry,elem) -> vaddr < hash_entry(b,struct vm_entry,elem) -> vaddr;
}

bool insert_vme(struct hash* vm, struct vm_entry* vme)		
{
	return hash_insert(vm,&vme->elem) == NULL ;
}

bool delete_vme(struct hash* vm, struct vm_entry* vme) 			
{
	hash_delete(vm,&vme->elem);
	return true;
}

struct vm_entry *find_vme(void* vaddr) 									
{
	struct hash_elem *e;
	struct vm_entry vme;

	vme.vaddr = pg_round_down(vaddr);
	e = hash_find(&thread_current()->vm,&vme.elem);
	if(e == NULL)
		return NULL;
	return hash_entry(e, struct vm_entry, elem);
}

void vm_destroy(struct hash* vm) 											
{
	hash_destroy(vm,vm_destroy_func);
}

void vm_destroy_func(struct hash_elem *e, void *aux UNUSED)	
{
	struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
	
	if(vme->is_loaded)
	{
		palloc_free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
		pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
	}
	free(vme);
}

bool load_file(void *kaddr, struct vm_entry *vme)						
{
	if(vme->read_bytes > 0)
	{
		lock_acquire(&filesys_lock);
		if((unsigned int)(vme->read_bytes) == file_read_at(vme->file, kaddr, vme->read_bytes, vme->offset))
		{
			lock_release(&filesys_lock);
			memset(kaddr + vme->read_bytes, 0, vme->zero_bytes);					
		}
		else
		{
			lock_release(&filesys_lock);
			return false;		
		}
	}
	else
	{
		memset(kaddr,0,PGSIZE);
	}

	return true;
}

















