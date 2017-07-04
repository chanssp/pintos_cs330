#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
struct lock file_lock;
void is_valid_pointer (void *esp);

#endif /* userprog/syscall.h */
