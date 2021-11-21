#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
typedef void (*CALL_PROC)(struct intr_frame*);
CALL_PROC syscalls[21];
void write(struct intr_frame* f); /* syscall write */
void halt(struct intr_frame* f); /* syscall halt. */
void exit(struct intr_frame* f); /* syscall exit. */
void exec(struct intr_frame* f); /* syscall exec. */

/* Our implementation for Task3: syscall create, remove, open, filesize, read, write, seek, tell, and close */
void create(struct intr_frame* f); /* syscall create */
void remove(struct intr_frame* f); /* syscall remove */
void open(struct intr_frame* f);/* syscall open */
void wait(struct intr_frame* f); /*syscall wait */
void filesize(struct intr_frame* f);/* syscall filesize */
void read(struct intr_frame* f);  /* syscall read */
void seek(struct intr_frame* f); /* syscall seek */
void tell(struct intr_frame* f); /* syscall tell */
void close(struct intr_frame* f); /* syscall close */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
 syscalls[SYS_WRITE] = &write; 
 syscalls[SYS_EXEC] = &exec;
  syscalls[SYS_HALT] = &halt;
  syscalls[SYS_EXIT] = &exit;
 
  // /* Our implementation for Task3: initialize create, remove, open, filesize, read, write, seek, tell, and close */
  syscalls[SYS_WAIT] = &wait;
  syscalls[SYS_CREATE] = &create;
  syscalls[SYS_REMOVE] = &remove;
  syscalls[SYS_OPEN] = &open;
  syscalls[SYS_SEEK] = &seek;
  syscalls[SYS_TELL] = &tell;
  syscalls[SYS_CLOSE] =&close;
  syscalls[SYS_READ] = &read;
  syscalls[SYS_FILESIZE] = &filesize;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  int *p=f->esp;
  int type = * (int *)f->esp;//检验系统调用号sys_code是否合法
  if(type <= 0 || type >= 21){
    printf ("system error!\n");
    thread_exit ();
  }
  syscalls[type](f);
}
void 
write (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;

  *user_ptr++;
  int fd = *user_ptr;
  const char * buffer = (const char *)*(user_ptr+1);
  off_t size = *(user_ptr+2);
  if (fd == 1) {//writes to the console
    //putbuf("gao",3);
    putbuf(buffer,size);
    f->eax = size;//return number written
  }
  
}
void halt(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void exit(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void exec(struct intr_frame* f){uint32_t *user_ptr = f->esp;}

/*
 * wll update wait,create,remove
 * */
void wait(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
/*
 * 在P2/pintos/src/filesys/filesys.c 里面有个filesys_create函数,因为不推荐修改
 * */
void create(struct intr_frame* f){
    uint32_t *user_ptr = f->esp;
    *user_ptr++;
    
}
void remove(struct intr_frame* f){uint32_t *user_ptr = f->esp;}

void open(struct intr_frame* f){uint32_t *user_ptr = f->esp;}

void filesize(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void read(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void seek(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void tell(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void close(struct intr_frame* f){uint32_t *user_ptr = f->esp;}