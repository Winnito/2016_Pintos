#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

struct lock filesys_lock; //add to use filesystem lock

#endif /* userprog/syscall.h */
