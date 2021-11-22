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
#include "threads/synch.h"
static void syscall_handler (struct intr_frame *);
typedef void (*CALL_PROC)(struct intr_frame*);
static struct lock filelock ;
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
    /*wll update. 对于错误，返回应该是-1*/
    thread_current()->exitStatus = -1;
    thread_exit ();
  }
  syscalls[type](f);
}

/* wll update.
* 判断指针指向的地址是否合法.用法就是在取参数前，把指向参数的指针传进来检查这个是不是合法
* 包括需要判断是否是用户地址；系统调用函数的范围[0,12](这个在handler里已经判断了)
* 如果一个指针，指向了没有权限的位置，就应该直接以-1退出。文档的3.1.5部分给我们提供了两种思路，一种是userprog/pagedir.c 和 in threads/vaddr.h中的相关函数pagedir_get_page，来验证地址范围，一种是通过访问该地址，来造成page_fault，在page_fault处理函数中，再退出。
* 采用第二种方法，还需修改page_fault
* */
void * checkPtr(const void *user_ptr){
  //是否指向了没有权限的位置
  if(!is_user_vaddr(user_ptr)){ 
    thread_current()->exitStatus = -1;
    thread_exit();
  }
  //采用第二种方法，检查用户指针是否指向下方PHYS_BASE，然后取消引用它.
  uint8_t * uaddr = (uint8_t *)user_ptr;
  for(uint8_t i = 0; i<4;i++){
    if(get_user(uaddr + i)==-1){
      thread_current()->exitStatus = -1;
      thread_exit();
    }
  }
}
/*下面是官方文档里给出的帮助代码*/

/* 在用户虚拟地址 UADDR 读取一个字节。
   UADDR 必须低于 PHYS_BASE。
   如果成功则返回字节值，如果出现段错误则返回 -1
   发生了。*/
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* 将 BYTE 写入用户地址 UDST。
   UDST 必须低于 PHYS_BASE。
   如果成功则返回真，如果发生段错误则返回假。*/
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}
//这些函数中的每一个都假定用户地址已经被验证为如下PHYS_BASE。他们还假设您已经进行了修改，page_fault()以便内核中的页面错误仅设置eax为0xffffffff并将其以前的值复制到eip.



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
/*wll update.这里需要记录进程退出的状态。*/
void exit(struct intr_frame* f){uint32_t *user_ptr = f->esp;uint32_t *user_ptr2 = f->esp;user_ptr2++;thread_current()->exitStatus = *user_ptr2;}
void exec(struct intr_frame* f){uint32_t *user_ptr = f->esp;}

/*
 * wll update wait,create,remove
 * uint32_t *user_ptr = f->esp; -----esp此时指向栈顶
 * user_ptr++;--------指针指向第一个参数，再加一就是第二个参数
 * 是否还需要判断指针的合法性？
 * */
/*
* 需要调用P2\pintos\src\userprog\process.c的process_wait (tid_t child_tid UNUSED) 
* */
void wait(struct intr_frame* f){
  uint32_t *user_ptr = f->esp;
  checkPtr(user_ptr + 1 );
  user_ptr ++;
  f->eax = process_wait(*user_ptr);
}
/*
 * 在P2/pintos/src/filesys/filesys.c 里面有个
 * filesys_create (const char *name, off_t initial_size)函数，直接调用
 * */
void create(struct intr_frame* f){
    uint32_t *user_ptr = f->esp; 
    checkPtr(user_ptr + 1);
    checkPtr(user_ptr + 2);
    user_ptr++;
    lock_acquire(&filelock);
    f->eax = filesys_create((const char*)*user_ptr,*(user_ptr+1));
    lock_release(&filelock);
}
/*
 * 在P2/pintos/src/filesys/filesys.c 里面有个
 * filesys_remove (const char *name) 函数，直接调用
 * */
void remove(struct intr_frame* f){
  uint32_t *user_ptr = f->esp;
  checkPtr(user_ptr + 1);
  checkPtr(*(user_ptr + 1));
  user_ptr++;
  lock_acquire(&filelock);
  f->eax = filesys_remove((const char*)*user_ptr);
  lock_release(&filelock);
  }

void open(struct intr_frame* f){uint32_t *user_ptr = f->esp;}

void filesize(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void read(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void seek(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void tell(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void close(struct intr_frame* f){uint32_t *user_ptr = f->esp;}