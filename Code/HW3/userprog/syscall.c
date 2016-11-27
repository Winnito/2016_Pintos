#include "threads/interrupt.h"
#include "userprog/syscall.h" 
#include <devices/shutdown.h> 
#include <devices/input.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h" 
#include <filesys/filesys.h> 
#include <filesys/file.h> 
#include "userprog/process.h" 
#include "threads/synch.h" 
#include "vm/page.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/swap.h"

static void syscall_handler (struct intr_frame *);

struct vm_entry* check_address(void *addr, void *esp);
void get_argument(void *esp, int *arg, int count);
void halt(void);
void exit(int status);
tid_t exec(const char *cmd_line);
int wait(tid_t tid);
bool create(const char *file, unsigned int initial_size);
bool remove(const char* file);
int open(const char *file);
int filesize(int fd);
int read(int fd, char *buffer, unsigned size);
int write(int fd, char *buffer, unsigned size);
void seek(int fd, unsigned int position);
unsigned int tell(int fd);
void close(int fd);

void
syscall_init (void){

	lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

}
/* check address */
struct vm_entry* check_address(void *addr, void *esp UNUSED)
{
	/*check address 0x08048000 ~ 0xc0000000 */ 
	bool loaded = false;

	if((unsigned int*)(addr) <= (unsigned int)(0x08048000) || (unsigned int*)addr >= (unsigned int)(0xc0000000))
  {
		exit(-1);
	}
	struct vm_entry* vme = find_vme(addr);

	if(vme)
	{
		handle_mm_fault(vme);
		loaded = vme->is_loaded;	
	}
	if(vme != NULL && loaded == false)
		exit(-1);

	return vme;
}

void check_valid_buffer(void *buffer, unsigned size, void *esp, bool to_write) 
{
	struct vm_entry *vme;
	unsigned int i;

	for(i=0; i<size; i++)
	{
		vme	= check_address(buffer,esp);
		
		if(vme == NULL)
			exit(-1);
	
		if(to_write && vme->writable == false)
			exit(-1);
		
		buffer += 1;
	} 
}

void check_valid_string(const void *str, void *esp) 
{
	struct vm_entry *vme;
	unsigned int i;
	unsigned int length;

	vme = check_address(str,esp);
	if(vme == NULL)
		exit(-1);

	length = strlen((char*)str);
	
	for(i=0; i<length; i++)
	{
		vme = check_address(str,esp);
		if(vme == NULL)
			exit(-1);
	}

}


/* get argument */
void get_argument(void *esp, int *arg, int count)
{
	int i;
	for(i = 0; i < count; i++){
		check_address(esp,esp);
		arg[i] = *(int*)esp;
		esp = esp + 4;
	}
}

int mmap(int fd, void *addr) 
{
	struct file* mmap_fp;
	struct file* mmap_refp;
	struct mmap_file *file;	
	
	mmap_fp = process_get_file(fd);

	if(mmap_fp == NULL)
		return -1;

	if((unsigned int)addr <= (unsigned int)0x08048000 || (unsigned int)addr >= (unsigned int)0xc0000000)
		return -1;

	if((unsigned int)addr % PGSIZE)
		return -1;
	
	mmap_refp = file_reopen(mmap_fp);	
	int length = file_length(mmap_refp);

	if(mmap_refp == NULL || length == 0)
		return -1;

	int mapid = thread_current()-> mapid++;

	file = (struct mmap_file*)malloc(sizeof(struct mmap_file));
	list_init(&file->vme_list);
	file->file = mmap_refp;
	file->mapid = mapid;	
	
	int offt = 0;
		
	while(length > 0)
	{
		/* set read_bytes and zero bytes */
		unsigned int read_bytes = length > PGSIZE ? PGSIZE : length;
		unsigned int zero_bytes = PGSIZE - read_bytes;

		if(find_vme(addr) != NULL)	 //if vme not exist
		{
			munmap(mapid); //relieve memory
			return -1;
		}
		
		/* allocate memory */
		struct vm_entry *vme = (struct vm_entry*)malloc(sizeof(struct vm_entry));
		if(vme == NULL) //if allocate fail		
		{ 
			munmap(mapid);
			return -1;
		}
	
		/* intialize entry variable */
		vme->file = mmap_refp;
		vme->offset = offt;
		vme->vaddr = addr;
		vme->read_bytes = read_bytes;
		vme->zero_bytes = zero_bytes;
		vme->type = VM_FILE;
		vme->writable = true;
		vme->is_loaded = false;

		/* insert entry into vme_list */
		list_push_back(&file->vme_list, &vme->mmap_elem);
		insert_vme(&thread_current()->vm, vme);

		/* change variable */
		length -= read_bytes;
		offt += read_bytes;
		addr += PGSIZE;
	}
	
	list_push_back(&thread_current()->mmap_list,&file->elem);
	
	return mapid;
}


/* file memory unmapping */
void
munmap(int mapping)
{
	
	/* will temporary store each object in child list */
	struct list_elem *elem;
	struct list_elem *next_elem;

	for(elem = list_begin(&thread_current()->mmap_list); elem != list_end(&thread_current()->mmap_list); elem = next_elem)
	{
		/* get_next element */
		next_elem = list_next(elem);

		/* get mmap_file from list */
		struct mmap_file *mmap_fp = list_entry(elem, struct mmap_file, elem);

		/* if mmap_file has target mapid or trying to unmap, unmapping */
		if(mmap_fp->mapid == mapping || mapping == -1)
		{
			do_munmap(mmap_fp);

			/* close file */
			if(mmap_fp->file != NULL)
			{
				lock_acquire(&filesys_lock);
				file_close(mmap_fp->file);
				lock_release(&filesys_lock);
			}

			list_remove(&mmap_fp -> elem);
			free(mmap_fp);
		}
	}
}

/* free vm_memory allocated for mmap */
void
do_munmap(struct mmap_file *mmap_f){

	/* will temporary store each object in child list */
	struct list_elem *elem;
	struct list_elem *next_elem;

	/* search vme list and delete them */
	for(elem = list_begin(&mmap_f->vme_list); elem != list_end(&mmap_f->vme_list); elem = next_elem)
	{
		/* get next element */
		next_elem = list_next(elem);

		/* get vm entry */
		struct vm_entry *vme = list_entry(elem, struct vm_entry, mmap_elem);

		if(vme != NULL && vme -> is_loaded)
		{
			if(pagedir_is_dirty(thread_current()->pagedir, vme->vaddr))
			{
				lock_acquire(&filesys_lock);
				file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
				lock_release(&filesys_lock);
			}

			/* clear page allocated for vm entry */
//			palloc_free_page(pagedir_get_page(thread_current() -> pagedir, c_entry -> vaddr));
			free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
			pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
		}

		/* remvoe from mmap list */
		list_remove(&vme->mmap_elem);

		/* delete entry from hash table */
		delete_vme(&thread_current()->vm, vme);

	}
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
	void *esp = (void*)(f->esp);
	int arg[3];
	check_address(esp,esp);
	int sys_call_number = *(int*)esp;
	esp = esp + 4;
	check_address(esp,esp);
	switch(sys_call_number)
  {
		case SYS_HALT:
			halt();		
			break;
		case SYS_EXIT:
			{
				get_argument(esp, arg, 1);
				int exit_status;
				exit_status = arg[0]; 
				exit(exit_status);
			}
			break;

		case SYS_EXEC:
			{
				get_argument(esp, arg, 1); 
				char *exec_cmd_line;
				exec_cmd_line = (char*)(arg[0]);
        //check_address(exec_cmd_line);
				check_valid_string(exec_cmd_line,esp);
				f->eax = exec(exec_cmd_line);
			}
			break;

		case SYS_WAIT:
			{
				get_argument(esp, arg, 1);
				int wait_tid = arg[0]; 
 				f->eax = wait(wait_tid);
			}
			break;

		case SYS_CREATE:
			{			
				get_argument(esp, arg, 2); 
				char *create_file_name;
				create_file_name  = (char*)(arg[0]);
				unsigned int create_initial_size;
				create_initial_size = (unsigned int)(arg[1]);
				//check_address(create_file_name);
				check_valid_string(create_file_name,esp);				
				f->eax = create(create_file_name, create_initial_size);
			}
			break;

		case SYS_REMOVE:		
			{	
				get_argument(esp, arg, 1); 
				char *remove_file_name;
				remove_file_name = (char*)(arg[0]);
				//check_address(remove_file_name);
				check_valid_string(remove_file_name,esp);				
				f->eax = remove(remove_file_name);
			}
			break;

		case SYS_OPEN:
			{
				get_argument(esp, arg, 1);
				char *open_file_name;		
				open_file_name = (char*)(arg[0]);
        //check_address(open_file_name);
				check_valid_string(open_file_name,esp);
				f->eax = open(open_file_name);
			}
			break;

		case SYS_FILESIZE:
			{
				get_argument(esp, arg, 1); 
				int fd;
				fd = arg[0]; 
				f->eax = filesize(fd);
			}
			break;

		case SYS_READ:
			{
				get_argument(esp, arg, 3); 
				int fd;
				char *buffer;
				unsigned size;

				fd = arg[0]; 
				buffer = (char*)arg[1];
				size = (unsigned)(arg[2]); 
				
				//check_address((void*)buffer);
				check_valid_buffer((void*)buffer,size,esp,false);				
				f->eax = read(fd, buffer, size);
			}
			break;

		case SYS_WRITE:
			{
				get_argument(esp, arg, 3);

				int fd = arg[0];
				char *buffer;
        unsigned size;
			
				fd = arg[0]; 
				buffer = (char*)arg[1];	
				size = (unsigned)arg[2]; 

				//check_address((void*)buffer);
				check_valid_buffer((void*)buffer,size,esp,false);        
				f->eax = write(fd, buffer, size);
			}
			break;

		case SYS_SEEK:
			{
				get_argument(esp, arg, 2); 

				int fd;
				unsigned int position;

				fd = arg[0]; 
				position = (unsigned int)(arg[1]);

				seek(fd, position);
			}
			break;

		case SYS_TELL:
			{
				get_argument(esp, arg, 1);
				
				int fd;
				fd = arg[0]; 

				f->eax = tell(fd);
			}
			break;

		case SYS_CLOSE:
			{
				get_argument(esp, arg, 2);		
				int fd;
				fd = arg[0];
				close(fd);
			}
			break;

		case SYS_MMAP:
			{
				get_argument(esp, arg, 2);
				int fd = arg[0];
				char *buffer = (char*)arg[1];
				
				f->eax = mmap(fd,(void*)buffer);
			}
			break;
		
		case SYS_MUNMAP:
			{
				get_argument(esp, arg, 1);
				int mapping = arg[0];
				munmap(mapping);
			}
			break;
	}		

}


/* shut down pintos */
void halt(void)
{
	shutdown_power_off();
}

/* exit current thread */
void exit(int status)
{
	struct thread *t = thread_current();
	printf("%s: exit(%d)\n", t->name, status);
	t -> exit_status = status; 
	thread_exit();
}

/* make child process, execute program */
tid_t exec(const char *cmd_line)
{
	tid_t tid = process_execute(cmd_line); 
	struct thread *child = get_child_process(tid); 
	if(child)
  { 
		sema_down(&child->sema_load); 
		if(child->load_succeed){ 
			return tid;
		}
	}
	return -1;
}


/* wait proces */
int wait(tid_t tid)
{
	return process_wait(tid);
}

/* create file */
bool create(const char *file, unsigned int initial_size)
{

	bool result;
	result = filesys_create(file, initial_size);
	return result;
}

/* remove file*/
bool remove(const char *file)
{
	bool result;
	result = filesys_remove(file);
	return result;
}

/* open file */
int
open(const char *file)
{
	struct file *open_file = filesys_open(file); 
	if(!open_file)
			return -1;
	
	int open_file_fd = process_add_file(open_file); 
	return open_file_fd;
}

/* search file object */
int filesize(int fd)
{	
	struct file *f = process_get_file(fd); 
	if(!f)
		return -1;	

	int file_size = file_length(f); 
	return file_size;
}

/* read data on open_file */
int read(int fd, char *buffer, unsigned size)
{
	lock_acquire(&filesys_lock); 
	if(fd == 0)
  {
		unsigned int i;
		for(i = 0; i < size; i++)
			buffer[i] = input_getc();
		
		lock_release(&filesys_lock);

		return size;
	}

	struct file *read_file = process_get_file(fd); 
	
	if(!read_file)
  {
		lock_release(&filesys_lock);
		return -1;
	}
	int read_bytes = file_read(read_file, buffer, size);
	lock_release(&filesys_lock); 

	return read_bytes;
}

/* write data on open_file */
int write(int fd, char *buffer, unsigned size)
{
	lock_acquire(&filesys_lock); 

	if(fd == 1)
  { 
		putbuf(buffer,size);
		lock_release(&filesys_lock); 
		return size;
	}

	struct file *write_file = process_get_file(fd); 

	if(!write_file)
  {
		lock_release(&filesys_lock);
		return -1;
	}

	int write_bytes = file_write(write_file, buffer, size);
	lock_release(&filesys_lock); 
	
  return write_bytes;
}

/* Move offset of file */
void seek(int fd, unsigned int position)
{	
	struct file *seek_file = process_get_file(fd); 
	if(!seek_file)
		return;
	
	file_seek(seek_file, (off_t)position);
}

/* get current offset of file */
unsigned int tell(int fd)
{
	struct file *tell_file = process_get_file(fd); 
  if(!tell_file)
		return -1;
	
	off_t offset = file_tell(tell_file);

	return offset;
}

/* close file */
void close(int fd)
{
	process_close_file(fd);
}
