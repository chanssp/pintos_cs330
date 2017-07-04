#include "userprog/syscall.h"
#include <stdio.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdlib.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"


typedef int pid_t;


static void syscall_handler (struct intr_frame *);

/*------------check-------------*/
/*----user defined functions----*/
//static void is_valid_pointer (void *esp);
static void get_arguments (int *esp, int *arg, int count);
static struct file_member * find_file_member (int fd);

static void syscall_halt(void);
static void syscall_exit(int status);
static pid_t syscall_exec (const char *cmd_line);
static int syscall_wait (pid_t pid);
static bool syscall_create (const char *file, unsigned initial_size);
static bool syscall_remove (const char *file);
static int syscall_open (const char *file);
static int syscall_filesize (int fd);
static int syscall_read (int fd, void *buffer, unsigned size);
static int syscall_write (int fd, const void *buffer, unsigned size);
static void syscall_seek (int fd, unsigned position);
static unsigned syscall_tell (int fd);
static void syscall_close (int fd);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int argv[4];
  int * esp = f->esp;
  int syscall_number; 

  is_valid_pointer(f->esp);

  syscall_number = *esp; 

  switch(syscall_number)
  {
    case SYS_HALT :  
      syscall_halt();
      break;
    case SYS_EXIT:  
      get_arguments(esp, &argv[0], 1);
      // printf("<exit>");
      syscall_exit((int)argv[0]);
      break;
    case SYS_EXEC :  
      get_arguments(esp, &argv[0], 1);
      // printf("<exec>");
      f->eax=syscall_exec((const char *)argv[0]);
      break;
    case SYS_WAIT :  
      get_arguments(esp, &argv[0], 1);
      // printf("<wait>");
      f->eax=syscall_wait((pid_t) argv[0]);
      break;      
    case SYS_CREATE :  
      get_arguments(esp, &argv[0], 2);
      //printf("<create>");
      is_valid_pointer ((int)argv[0]);
      f->eax=syscall_create((const char *)argv[0],(unsigned) argv[1]);
      break;
    case SYS_REMOVE :  
      get_arguments(esp, &argv[0], 1);
      //printf("<remove>");
      is_valid_pointer ((int)argv[0]);
      f->eax=syscall_remove((const char *)argv[0]);
      break;
    case SYS_OPEN :  
      get_arguments(esp, &argv[0], 1);
      //printf("<open>");
      is_valid_pointer ((int)argv[0]);
      f->eax=syscall_open((const char *)argv[0]);
      break;
    case SYS_FILESIZE :  
      get_arguments(esp, &argv[0], 1);
      //printf("<size>");
      f->eax=syscall_filesize((int)argv[0]);
      break;
    case SYS_READ : 
      //printf("<read>");
      get_arguments(esp, &argv[0], 3);
      //printf("<after-r>");
      is_valid_pointer ((int)argv[1]);
      f->eax=syscall_read((int) argv[0],(void *) argv[1],(unsigned) argv[2]);
      break;
    case SYS_WRITE :  
      //printf("<write>");
      get_arguments(esp, &argv[0], 3);
      //printf("<after-w>");
      is_valid_pointer ((int)argv[1]);
      f->eax=syscall_write((int) argv[0],(const void *) argv[1],(unsigned) argv[2]);
      break;
    case SYS_SEEK :  
      get_arguments(esp, &argv[0], 2);
      //printf("<seek>");
      syscall_seek((int)argv[0],(unsigned) argv[1]);
      break;
    case SYS_TELL :  
      get_arguments(esp, &argv[0], 1);
      //printf("<tell>");
      f->eax=syscall_tell((int) argv[0]);
      break;
    case SYS_CLOSE :  
      get_arguments(esp, &argv[0], 1);
      //printf("<close>");
      syscall_close((int) argv[0]);
      break;   
  }

}

/*---------check-----------*/
/*----syscall functions----*/
static void
syscall_halt(void)
{
  power_off ();
}

static void
syscall_exit(int status)

{
  struct child_member * child;
  struct thread * cur = thread_current();
  char * name = cur->name;
  //for rox problems
  struct list_elem * e;
  struct file_member * file;

  child = get_child_process(cur->tid);
  // printf(" called by %d, in exit, ", cur->tid);
  if(child==NULL) {
    return -1;
  }

  child->used = true;
  child-> exit_status = status;

  if (cur->parent->waiting_for == cur->tid)
    sema_up(&cur->parent->wait_sema);
  
  while(!list_empty(&cur->file_list))
  {
    e = list_begin(&cur->file_list);
    file= list_entry(e, struct file_member, file_elem);
    syscall_close(file->fd);
  }

  file_close(cur->running_file);
  
  printf("%s: exit(%d)\n", name, status);
  thread_exit();
}

static pid_t 
syscall_exec (const char *cmd_line)
{ 
  pid_t tid = process_execute(cmd_line);

  return tid;
}

static int 
syscall_wait (pid_t pid)
{
  return process_wait(pid);
}

static bool 
syscall_create (const char *file, unsigned initial_size)
{
  bool success;
  success = filesys_create (file, initial_size);
  return success;
}

static bool 
syscall_remove (const char *file)
{
  bool success;
  success = filesys_remove (file);
  return success; 
}

static int 
syscall_open (const char *file)
{ 
  static int fd = 2;
  struct file * open_file = filesys_open(file);
  struct file_member * fm;
  struct thread * cur = thread_current();

  if(open_file==NULL){
    return -1;
  }
  
  else {
    fm = (struct file_member *) malloc(sizeof(struct file_member));
    fm->file_ptr = open_file;
    fm->fd = fd;
    fd++;
  }
  list_push_back(&cur->file_list, &fm->file_elem);

  return fm->fd;
}

static int 
syscall_filesize (int fd)
{
  struct file_member * fm = find_file_member(fd);
  int size;
  struct file * open_file;

  if (fm == NULL) {
    return -1;
  }

  open_file = fm->file_ptr;
  size = (int) file_length(open_file);
  return size;
}

static int 
syscall_read (int fd, void *buffer, unsigned size)
{
  struct file_member * open_file_member;
  struct file * open_file;
  int actual_read, i;

  if (fd == 0)
  {
    for(i=0;i<size;i++)
      *((char *)buffer+i) = input_getc();

    return size;
  }

  open_file_member = find_file_member(fd);
  if (open_file_member == NULL){
    return -1;
  }
  
  open_file = open_file_member->file_ptr;
  actual_read = (int) file_read(open_file,buffer,size);
  return actual_read;
}

static int 
syscall_write (int fd, const void *buffer, unsigned size)
{
  struct file_member * open_file_member;
  struct file * open_file;
  int actual_write;

  if(fd==1){
    putbuf(buffer,size);
    return size;
  }

  open_file_member = find_file_member(fd);
  if (open_file_member == NULL){
    return -1;
  }
  
  open_file = open_file_member->file_ptr;
  actual_write = (int) file_write(open_file,buffer,size);
  return actual_write;

}

static void 
syscall_seek (int fd, unsigned position)
{
  struct file * open_file;
  open_file = find_file_member(fd)->file_ptr;

  if (open_file == NULL)
    return;
  file_seek(open_file, position);
}

static unsigned 
syscall_tell (int fd)
{
  unsigned pos;
  struct file * open_file;
  open_file = find_file_member(fd)->file_ptr;

  if (open_file == NULL)
    return 
  -1;
  pos = (unsigned) file_tell(open_file);
  return pos;
}

static void
syscall_close (int fd)
{
  struct file_member * fm = find_file_member(fd);
  if(fm){
    file_close(fm->file_ptr);
    list_remove(&fm->file_elem);
    free(fm);
    return;
  }
  else{
    return;
  }
}

/*----------check----------*/
/*----helper functions-----*/

void
is_valid_pointer (void *esp) 
{ 
  void * check_ptr; 

  if(!(is_user_vaddr(esp))) {
    syscall_exit(-1);
  }

  if (esp == NULL) {
    syscall_exit(-1);
  }
  
  check_ptr = pagedir_get_page(thread_current()->pagedir, esp);

  if (!check_ptr) {
    syscall_exit(-1);
  }
  

}

static void
get_arguments (int *esp, int *argv, int count)
{
  int * arg_address;
  int i;

  for(i=0;i<count;i++)
  {
    esp++;
    is_valid_pointer(esp);
    arg_address = (int *) esp;
    argv[i] = *arg_address;
  }
}


static struct file_member *
find_file_member (int fd)
{
  struct thread * cur = thread_current();
  struct list * files= &cur->file_list;
  struct list_elem * e;

  for (e = list_begin (files); e != list_end(files); e = list_next(e))
  {
      if (fd == list_entry(e, struct file_member, file_elem)->fd)
          return list_entry(e, struct file_member, file_elem);
  }

  return NULL;
}