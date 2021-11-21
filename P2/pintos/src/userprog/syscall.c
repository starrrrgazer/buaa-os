<<<<<<< HEAD
#include "userprog/syscall.h" 
#include "threads/vaddr.h" 
#include <stdio.h> 
#include <syscall-nr.h> 
#include "threads/interrupt.h" 
#include "threads/thread.h" 
#include "filesys/filesys.h" 
#include "filesys/file.h" 
#include "devices/input.h" 
#include  "process.h" 
#include <string.h> 
#include "devices/shutdown.h" 
#define MAXCALL 21 
#define MaxFiles 200 
#define stdin 1 
=======
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
>>>>>>> master

typedef void (*SysCall)(struct intr_frame *);
SysCall syscalls[MAXCALL];
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

void syscall_Write(struct intr_frame*); 
void syscall_Exit(struct intr_frame *f); 
void syscall_Create(struct intr_frame *f); 
void syscall_Close(struct intr_frame *f); 
void syscall_Exec(struct intr_frame *f); 
void syscall_Wait(struct intr_frame *f); 
void syscall_Seek(struct intr_frame *f); 
void syscall_Remove(struct intr_frame *f); 
void syscall_Tell(struct intr_frame *f); 
void syscall_Halt(struct intr_frame *f); 



void ExitStatus(int status); 
void syscall_Open(struct intr_frame *f); 
void syscall_FileSize(struct intr_frame *f);
void syscall_Read(struct intr_frame *f); 

struct file_node *GetFile(struct thread *t,int fd)   //依据文件句柄从进程打开文件表中找到文件指针
{ 
  struct list_elem *e; 
  for(e=list_begin(&t->file_list);e!=list_end(&t->file_list);e=list_next(e)) 
  { 
    struct file_node *fn = list_entry (e, struct file_node, elem); 
    if(fn->fd==fd) return fn; 

  } 
  return NULL; 

}


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  int i;
  for(i=0;i<MAXCALL;i++){
	syscalls[i]=null;
  }
 
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
//  printf ("system call!\n");

  if(!is_user_vaddr(f->esp)) ExitStatus(-1);
  //thread_exit ();

  int mark=*((int *)(f->esp)); 
  if(mark>=MAXCALL||MAXCALL<0) {
    printf("We don't have this System Call!\n"); 
    ExitStatus(-1); 
  } 
  if(sys[mark]==NULL) { 
    printf("this System Call %d not Implement!\n",No); 
    ExitStatus(-1); 
  } 

  sys[mark](f); 

}

void ExitStatus(int status)      //非正常退出时使用
{ 
  struct thread *cur=thread_current(); 
  cur->ret=status; 
  thread_exit(); 
}

void 
syscall_open (struct intr_frame* f)
{
  struct thread *cur=thread_current(); 
  const char *FileName=(char *)*((int *)f->esp+1);
  struct file_node *fn=(struct file_node *)malloc(sizeof(struct file_node));  

  if(!is_user_vaddr(((int *)f->esp)+2)) ExitStatus(-1); 

  if(FileName==NULL) 
  { 
    f->eax=-1; 
    ExitStatus(-1); 
  } 
  
  fn->f=filesys_open(FileName); 

  if(fn->f==NULL||cur->FileNum>=MaxFiles) fn->fd=-1; 

  else fn->fd=++cur->maxFd; 
  
  f->eax=fn->fd; 

  if(fn->fd==-1) free(fn); 

  else 
  { 
    cur->FileNum++; 
    list_push_back(&cur->file_list,&fn->elem); 
  }

}

void 
syscall_Read(struct intr_frame *f) 

{ 

  int *esp=(int *)f->esp; 
  int fd=*(esp+2); 
  char *buffer=(char *)*(esp+6); 
  unsigned size=*(esp+3); 
  struct thread *cur=thread_current(); 
  struct file_node *fn=NULL; 
  unsigned int i;

  if(!is_user_vaddr(esp+7)) ExitStatus(-1); 

  if(buffer==NULL||!is_user_vaddr(buffer+size)) 
  { 
    f->eax=-1; 
    ExitStatus(-1); 
  }  

  if(fd==STDIN_FILENO)               //从标准输入设备读
  { 
    for(i=0;i<size;i++) 
      buffer[i]=input_getc(); 
  } 

  else                            //从文件读
  { 
    fn=GetFile(cur,fd);         //获取文件指针
    if(fn==NULL) 
    { 
      f->eax=-1; 
      return; 
    } 

    f->eax=file_read(fn->f,buffer,size); 

  } 

} 




void 
syscall_FileSize(struct intr_frame *f) 
{  

  struct thread *cur=thread_current(); 
  int fd=*((int *)f->esp+1); 
  struct file_node *fn=GetFile(cur,fd); 

  if(!is_user_vaddr(((int *)f->esp)+2)) ExitStatus(-1);

  if(fn==NULL) 
  { 
    f->eax=-1; 
    return; 
  } 

  f->eax=file_length (fn->f); 

}
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

void create(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void remove(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void open(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void wait(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void filesize(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void read(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void seek(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void tell(struct intr_frame* f){uint32_t *user_ptr = f->esp;}
void close(struct intr_frame* f){uint32_t *user_ptr = f->esp;}

