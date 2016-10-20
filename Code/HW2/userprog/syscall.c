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

static void syscall_handler (struct intr_frame *);

void check_address(void *addr);
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
void check_address(void *addr)
{
	/*check address 0x08048000 ~ 0xc0000000 */
	if((unsigned int*)(addr) <= (unsigned int)(0x08048000) || (unsigned int*)addr >= (unsigned int)(0xc0000000))
  {
		exit(-1);
	}
}

/* get argument */
void get_argument(void *esp, int *arg, int count)
{
	int i;
	for(i = 0; i < count; i++){
		check_address(esp);
		arg[i] = *(int*)esp;
		esp = esp + 4;
	}
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
	void *esp = (void*)(f->esp);
	int arg[3];
	check_address(esp);
	int sys_call_number = *(int*)esp;
	esp = esp + 4;
	check_address(esp);
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
        check_address(exec_cmd_line);
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
				check_address(create_file_name);
				f->eax = create(create_file_name, create_initial_size);
			}
			break;

		case SYS_REMOVE:		
			{	
				get_argument(esp, arg, 1); 
				char *remove_file_name;
				remove_file_name = (char*)(arg[0]);
				check_address(remove_file_name);
				f->eax = remove(remove_file_name);
			}
			break;

		case SYS_OPEN:
			{
				get_argument(esp, arg, 1);
				char *open_file_name;		
				open_file_name = (char*)(arg[0]);
        check_address(open_file_name);
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
				
				check_address((void*)buffer);
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

				check_address((void*)buffer);
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
