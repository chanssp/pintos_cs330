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
#include <list.h>
#include "vm/frame.h"
#include "vm/page.h"


typedef int pid_t;
// typedef int mapid_t;

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
static mapid_t syscall_mmap (int fd, void *addr);
static void syscall_munmap (mapid_t mapping);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int argv[4];  // 스위치 문에서 인자를 받을 때 사용
  int * esp = f->esp;
  int syscall_number; 


  // 먼저 esp 가 가리키는 주소가 valid 한지 확인
  is_valid_pointer(f->esp);

  syscall_number = *esp;  // 스택 가장 위에 저장 되어있는 number. 이후에 esp 가 가리키는 것은 1st argument

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
      is_valid_pointer((int)argv[0]);
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
      if(!(is_user_vaddr((int)argv[1]))) {
        syscall_exit(-1);
      }
      if ((int)argv[1] == NULL) {
       syscall_exit(-1);
      }
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
    case SYS_MMAP :
      get_arguments (esp, &argv[0], 2);
      f->eax=syscall_mmap((int) argv[0], (void *) argv[1]);
      break;
    case SYS_MUNMAP :
      get_arguments (esp, &argv[0], 1);
      syscall_munmap ((mapid_t) argv[0]);
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

  child = find_self_from_parent(cur->tid);

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
  // printf("<<exec>>\n");
  pid_t tid = process_execute(cmd_line);

  return tid;
}

static int 
syscall_wait (pid_t pid)
{ 
  // printf("<<wait>>\n");
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
  // printf("<<open>>\n");
  struct file * open_file = filesys_open(file);
  struct file_member * fm;
  struct thread * cur = thread_current();

  if(open_file==NULL){
    return -1;
  }
  
  fm = (struct file_member *) malloc(sizeof(struct file_member));
  fm->file_ptr = open_file;
  fm->fd = fd;
  
  fd++;
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
  // printf("<<read>>\n");
    if (fd == 0)
  {
    for(i=0;i<size;i++){
      *((char *)buffer+i) = input_getc();
      }
    return size;
  }
  
  open_file_member = find_file_member(fd);
  if (open_file_member == NULL){
    return -1;
  }
  
  open_file = open_file_member->file_ptr;
  actual_read=file_read(open_file,buffer,size);
  
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
  // lock_acquire(&file_lock);
  actual_write = (int) file_write(open_file,buffer,size);
  // lock_release(&file_lock);
  
  return actual_write;

}

static void 
syscall_seek (int fd, unsigned position)
{
  
  struct file * open_file;

  if (open_file == NULL)
    return;

  open_file = find_file_member(fd)->file_ptr;

  file_seek(open_file, position);
  
}

static unsigned 
syscall_tell (int fd)
{
  
  unsigned pos;
  struct file * open_file;
  open_file = find_file_member(fd)->file_ptr;

  if (open_file == NULL){
    return -1;
  }
  pos = (unsigned) file_tell(open_file);
  
  return pos;
}

static void
syscall_close (int fd)
{
  
  struct file_member * fm = find_file_member(fd);
  // printf("<<close>>\n");
  struct thread * cur = thread_current();
  struct list_elem * e;
  struct mmap_member * mmem;
  int size,real_read;

  if (fm == NULL)
    return;
  for (e = list_begin (&cur->mmap_list); e != list_end(&cur->mmap_list); e = list_next(e)) {
    mmem = list_entry (e, struct mmap_member, mmap_elem);
    if (mmem->file_ptr == fm->file_ptr) {
      if (pagedir_is_dirty(cur->pagedir, mmem->spte->v_address)) {
        size = file_length(mmem->file_ptr);
        // printf ("size write: %d\n", size);
        real_read=file_write(mmem->file_ptr, pagedir_get_page(cur->pagedir,mmem->map_addr), size);
        file_seek(mmem->file_ptr,0);
        // printf("read : %d",real_read);
      }
    }
    break;
  }

  file_close(fm->file_ptr);
  list_remove(&fm->file_elem);
  free(fm);
  
}




// static mapid_t
// syscall_mmap (int fd, void *addr)
// {
//   struct file_member * open_file_member;
//   struct file * open_file;
//   void * frame;
//   struct mmap_member * mmem;
//   struct spt_entry * spte;
//   int size, ofs, actual_read;
//   bool insert_success;
//   struct thread * cur = thread_current();

//   static int mapid = 0;

//   if (fd == 0 || fd == 1) {
//     return -1;
//   }

//   // 현재 thread 에서 fd 에 해당하는 file 찾기.
//   open_file_member = find_file_member (fd);
//   if (open_file_member == NULL) {
//     return -1;
//   }
//   // printf("fd: %d\n", fd);
//   // printf ("file descriptor: %d\n", open_file_member->fd);

//   open_file = open_file_member->file_ptr;

//   size = file_length (open_file);
//   if (size <= 0) {
//     return -1;
//   }

//   // page alignment
//   if (!( ((int) addr) % PGSIZE)) {
//     pg_round_down (addr);
//   }

//   ofs = 0;
//   while (size>0)
//   {
//     size_t page_read_bytes = size < PGSIZE ? size : PGSIZE;
//     size_t page_zero_bytes = PGSIZE - page_read_bytes;

//     spte = (struct spt_entry *) malloc (sizeof (struct spt_entry));
//     spte = initialize_mmap_spte (spte, open_file, ofs, addr, page_read_bytes, page_zero_bytes);
//     insert_success = insert_into_spt(spte);

//     // printf("pass0\n");
//     // frame = allocate_frame (spte, PAL_USER);
//     // if  (frame == NULL) {
//     //   return -1;
//     // }
//     printf("<<*a : %p>>\n",*(int *)addr);
//      printf("pass1\n");

//     if (file_read(open_file, *(int*)addr, page_read_bytes) != (int) page_read_bytes) {
//        printf("pass2\n");
//       // munmap
//       return -1;
//     }


//     // printf("pass3\n");

//     memset (*(int*)

//       addr + page_read_bytes, 0, page_zero_bytes);
    
//     // printf("pass4\n");

//     if (!pagedir_get_page (cur->pagedir, spte->v_address)== NULL
//           && pagedir_set_page (cur->pagedir, spte->v_address, addr, true)) {
//       //munmap
//       return -1;
//     }

//     // printf("<<a : %p>>\n",addr);
//     //  printf("<<frame : %p>>\n",frame);
//     //  printf("<<*a : %p>>\n",*(int *)addr);
//     //  printf("<<*frame : %p>>\n",*(int *)frame);
//     // printf("pass5\n");

//     spte->valid = true;
//     ofs += PGSIZE;
//     addr += PGSIZE;
//     size-=page_read_bytes;
//   }

//   mapid ++;
//   mmem = (struct mmap_member *) malloc (sizeof(struct mmap_member));
//   mmem->mapid = mapid;
//   mmem->map_addr = addr;
//   // printf("pass6\n");
//   list_push_back (&cur->mmap_list, &mmem->mmap_elem);
//   // printf("pass7\n");

//   return mmem->mapid;
// }



static mapid_t
syscall_mmap (int fd, void *addr)
{
  struct file_member * open_file_member;
  struct file * open_file;
  void * frame;
  struct mmap_member * mmem;
   struct mmap_member * mm;
  struct spt_entry * spte;
  int size, ofs, actual_read,cnt;
  bool insert_success;
  struct thread * cur = thread_current();
  struct list_elem * e;
  static int mapid = 0;
  // printf("<<mmap>>\n");

  if (fd == 0 || fd == 1) {
    return -1;
  }

  if(addr == 0 || addr == 0x08048000 || addr == PHYS_BASE - 4096 || addr == 0x0804B000) {
    // catches: mmap null || mmap over code || mmap over stk || mmap over data
    return -1;
  }
  // 현재 thread 에서 fd 에 해당하는 file 찾기.
  open_file_member = find_file_member (fd);
  if (open_file_member == NULL) {
    return -1;
  }

  open_file = open_file_member->file_ptr;
  size = file_length (open_file);
  if (size <= 0) {
    return -1;
  }

  //for mmap overlap and mmap twice testcases
  for (e = list_begin (&cur->mmap_list); e != list_end(&cur->mmap_list); e = list_next(e)) {
    mm = list_entry (e, struct mmap_member, mmap_elem);
    if (mm->file_ptr=open_file) {
      if(mm->map_addr + size>addr){
        return -1;
      }
    }
  }


  // page alignment
  if (( ((int) addr) % PGSIZE)) {
    return -1;
  }

   ofs = 0;
   cnt=0;
  while (size>0)
  {
    size_t page_read_bytes = size < PGSIZE ? size : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;
    cnt++;
    // printf ("while loop: %d\n", cnt);
    spte = (struct spt_entry *) malloc (sizeof (struct spt_entry));
    spte = initialize_mmap_spte (spte, open_file, ofs, addr, page_read_bytes, page_zero_bytes);
    insert_success = insert_into_spt(spte);

    // printf("pass0\n");
    frame = allocate_frame (spte, PAL_USER);
    if  (frame == NULL) {
      return -1;
    }
    // printf("pass1\n");
    lock_acquire(&file_lock);
    if (file_read(open_file, frame, page_read_bytes) != (int) page_read_bytes) {
      lock_release(&file_lock);
      remove_frame(frame);
      // printf("pass2\n");
      // munmap
      return -1;
    }
    lock_release(&file_lock);
    // printf("pass3\n");

    memset (frame + page_read_bytes, 0, page_zero_bytes);
    
     // printf("pass4\n");

    if (!pagedir_set_page (cur->pagedir, spte->v_address, frame, true)) {
      //munmap
      return -1;
    }
    // printf("<<pass>>\n");


    // printf("<<a : %p>>\n",addr);
    // printf("<<frame : %p>>\n",frame);
    // printf("<<*a : %p>>\n",*(int *)addr);
    // printf("<<*frame : %p>>\n",*(int *)frame);

    
    // printf("pass5\n");
    spte->valid = true;
    ofs += PGSIZE;
    addr += PGSIZE;
    size-=page_read_bytes;
  }
  file_seek(open_file,0);
  mapid ++;
  mmem = (struct mmap_member *) malloc (sizeof(struct mmap_member));
  mmem->mapid = mapid;
  mmem->map_addr = addr-cnt*PGSIZE;
  mmem->spte=spte;
  mmem->file_ptr=open_file_member->file_ptr;
  // printf("pass6\n");
  list_push_back (&cur->mmap_list, &mmem->mmap_elem);
    // printf("pass7\n");

  return mmem->mapid;
}



static void
syscall_munmap (mapid_t mapping)
{
  struct thread * cur = thread_current();
  struct list_elem * e;
  struct mmap_member * mmem;
  void * frame;

  int size,real_read;
  
  // printf("<<unmap>>\n");
  for (e = list_begin (&cur->mmap_list); e != list_end(&cur->mmap_list); e = list_next(e)) {
    mmem = list_entry (e, struct mmap_member, mmap_elem);
    if (mmem->mapid == mapping) {

      if (pagedir_is_dirty(cur->pagedir, mmem->spte->v_address)) {
        size = file_length(mmem->file_ptr);
        // printf ("size write: %d\n", size);
        lock_acquire(&file_lock);
        real_read=file_write(mmem->file_ptr, pagedir_get_page(cur->pagedir,mmem->map_addr), size);
        lock_release(&file_lock);
        file_seek(mmem->file_ptr,0);

        // printf("read : %d",real_read);

      }

      frame = pagedir_get_page(cur->pagedir,mmem->map_addr);
      remove_frame(frame);
      list_remove (&mmem->mmap_elem);
      hash_delete(&cur->supp_page_table,&mmem->spte->spt_elem);
      free(mmem->spte);
      free(mmem);
    }
    break;
  }

}

/*----------check----------*/
/*----helper functions-----*/

void
is_valid_pointer (void *esp) 
{ 
  void * check_ptr; 

  if(!(is_user_vaddr(esp))) {
    // printf("ptr11-");
    syscall_exit(-1);
  }

  if (esp == NULL) {
    // printf("ptr22-");
    syscall_exit(-1);
  }
  
  check_ptr = pagedir_get_page(thread_current()->pagedir, esp);

  if (!check_ptr) {
    // printf("ptr33-");
    syscall_exit(-1);
  }
  

}

static int checker= 1;

static void
get_arguments (int *esp, int *argv, int count)
{
  int * arg_address;
  int i;

  checker++;

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

  //printf ("<%d>", fd);
  //printf("<<%d>>" , list_size(files));

  for (e = list_begin (files); e != list_end(files); e = list_next(e))
  {
      if (fd == list_entry(e, struct file_member, file_elem)->fd)
          return list_entry(e, struct file_member, file_elem);
  }
  //printf("NO FILDE DESCRIPTOR???");
  return NULL;
}