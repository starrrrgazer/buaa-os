#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads/malloc.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
void push_argument (void **esp, int argc, char * argv[]);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
//!线程创建函数 file_name包含了我们要执行的函数和后面的参数
//!修改大概就是将文件名和参数分开取出文件名传递给thread_create
{
  
  char *fn_copy; //!函数strtok_R会改变原符号串，所以要一份复制的
  char *myfn_copy;
  tid_t tid;
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  
  //!
  fn_copy=palloc_get_page(0);
  myfn_copy=palloc_get_page(1);
  strlcpy (fn_copy, file_name, strlen(file_name)+1);
  strlcpy (myfn_copy, file_name, strlen(file_name)+1);


  /* Create a new thread to execute FILE_NAME. */
  //!
  char *save_ptr;
  char *rname = strtok_r(myfn_copy, " ", &save_ptr);
  tid = thread_create (rname, PRI_DEFAULT, start_process, fn_copy);
  //!thread_create()函数创建一个内核线程用来执行这个线程
  palloc_free_page(myfn_copy);
  if (tid == TID_ERROR){
    palloc_free_page(fn_copy);
    return tid;
  }
//palloc_free_page(fn_copy);
 /* wll update .信号量修改和子进程运行的判断*/
  sema_down(&thread_current()->sema);//降低父进程的信号量，等待子进程创建结束
  //!
  palloc_free_page(fn_copy);
  if (!thread_current()->childSuccess) return TID_ERROR;//子进程加载可执行文件失败报错 
  return tid;
}



/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
//!
  
   char *fn_copy;
   fn_copy=palloc_get_page(0);
  strlcpy(fn_copy,file_name,strlen(file_name)+1);

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  //!
  //!分割文件名
  char *save_ptr;
  char *rname;
  rname=strtok_r(file_name," ",&save_ptr);
  //printf("(arg0)%s\n",*file_name);
  //!
  //!加载成功successs为ture，且load初始化esp
  success = load (rname, &if_.eip, &if_.esp);
  if (!success){
   palloc_free_page (fn_copy);
   thread_current()->parent->childSuccess = false; //! 父进程记录子进程的失败
   sema_up(&thread_current()->parent->sema); //!增加父进程的信号量，通知父进程
    thread_exit ();
  }
    
    char* esp=if_.esp;
    char* argv[300];
    char* argv1[300];
    int i,j,espmove,argc=0;
    char* name;
    ;
    for(name=strtok_r(fn_copy," ",&save_ptr);name!=NULL;name=strtok_r(NULL," ",&save_ptr)){
      argv1[argc]=name;
      //printf("(arg00)%s\n",argv1[argc]);
      //argv[argc]=esp;
      //printf("(arg1)%s %#X\n",name,(int *)argv[argc]);
      argc++;
    }
    i=argc-1;
    for(i=argc-1;i>=0;i--){
       espmove=strlen(argv1[i])+1;
       esp-=espmove;
       memcpy (esp, argv1[i], strlen(argv1[i])+1);
    //   printf("(arg1)%s %#X\n",esp,esp);
       argv[i]=esp;
    //  //printf("(arg00)%#X\n",(int *)argv[i]);
    }
    while((int)esp%4!=0){
      esp--;
    }
    esp = esp -4;
  (*(int*)esp)  = 0;
    //printf("(arg2)%d %#X\n",*(int*)esp,esp);
    int *p=esp;
    p--;
    for(i=argc-1;i>=0;i--){
      *p=(int *)argv[i];
      //printf("(arg3)%#X %#X\n",*p,(int *)p);
      p--;
    }
    *p=p+1;
    //printf("(arg4)%#X %#X\n",*p,(int *)p);
    p--;
    *p=argc;
    //printf("(arg5)%d %#X\n",*p,(int *)p);
    p--;
    *p=0;
    //printf("(arg6)%d %#X\n",*p,(int *)p);
    if_.esp=p;
    palloc_free_page(fn_copy);
     thread_current ()->parent->childSuccess = true;//! 父进程记录子进程的成功
    sema_up (&thread_current ()->parent->sema);//! 增加父进程的信号量，通知父进程




  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */

/*
* wll update.
* 如果pid仍然存在，则等待它终止。然后，返回pid传递给的状态exit。如果pid没有调用exit()，而是被内核终止（例如，由于异常而终止），则wait(pid)必须返回 -1。
* wait 如果以下任何条件为真，则必须失败并立即返回 -1：
* 1.pid 不引用调用进程的直接子进程。 pid 是调用进程的直接子进程，当且仅当调用进程收到 pid 作为成功调用 exec 的返回值。
请注意，子进程不会被继承：如果 A 产生子进程 B 并且 B 产生子进程 C，那么 A 不能等待 C，即使 B 已经死了。 进程 A 对 wait(C) 的调用必须失败
。 类似地，如果孤立进程的父进程在它们退出之前退出，则不会将孤立进程分配给新的父进程。
翻译一下：就是子进程的不是当前进程的子进程,返回-1
* 2.调用wait的进程已经调用了wait on pid。 也就是说，一个进程最多可以等待任何给定的孩子一次。
* 3.被内核终止。在exception.c kill()中可以发现当出现意外结束时，存在判断，调用了thread exit
* 4.否则返回线程退出的状态
* 这里我们要保证子进程一定成功创建，就需要实现一个同步锁，来保证子进程load成功才接着执行父进程，子进程一旦创建失败，说明该该调用失败了。
*	而对于wait操作，很明显也需要一个锁，保证父进程在子进程执行期间无法进行任何操作，等待子进程退出后，父进程获取子进程退出码，并回收资源。
这里的锁的设计非常精妙，要保证父进程wait时，无法执行任何操作，子进程退出时，需要立刻通知父进程，但不能直接销毁，而要等待父进程来回收资源获取返回码等，然后才可以正常销毁。
* 
* 1.process_execute 是线程创建函数，里面调用了thread_create
* 为了判断进程id到底是不是子进程，需要一个记住子进程的队列，这一部分要在thread_create里面初始化
* 2.为了实现“子进程一定成功创建”，在子进程创建时调用的start pocess里对load的结果success额外做了补充。首先是父进程在process execute里降低信号量，
等待子进程创建成功的消息，然后子进程在创建时调用的start process用sema up父进程信号量来实现同步锁
* 3.为了实现“父等子，子通知父”，当父进程调用wait等待子进程时，需要sema down子进程的信号量，然后在子进程exit时再sema up 子进程的信号量，这样就实现了通知父进程
* 所以要在process_execute 降低父进程信号量 ， 在子进程的start_process增加父进程的信号量
* 4.为了实现进程最多等待任何给定的子进程一次，多加了一个waited变量来判断这个进程是否已被wait过
* 5.为了实现内核终止返回-1，这里需要检查指针，如果是中断的话，就将返回状态改为-1
*/
int
process_wait (tid_t child_tid UNUSED) 
{
  struct list *childs = &thread_current()->childs;//当前进程的子进程们
  struct list_elem *childElem;
  childElem = list_begin (childs);
  struct child *childPtr = NULL;
  /*这个循环一直执行直到找到子进程中父进程正在等待的那个（child_tid）*/
  while (childElem != list_end (childs))
  {
    childPtr = list_entry (childElem, struct child, child_elem);
    if (childPtr->tid == child_tid)
    {//找到了正在等待的子进程
      if (!childPtr->waited)//这个子进程是否被等待过
      {
        childPtr->waited = true;
        sema_down (&childPtr->sema);//等待子进程运行结束
        break;//如果正在等待的那个子进程没有在运行了，则减少它的信号量（为了唤醒父进程）
      } 
      else //子进程被等待过了，返回-1
      {
        return -1;
      }
    }
    childElem = list_next (childElem);
  }
  if (childElem == list_end (childs)) {
    return -1;//没有找到对应子进程，返回-1
  }
  //执行到这里说明子进程正常退出
  list_remove (childElem);//也是释放资源
  return childPtr->exitStatus;//返回子进程的退出状态
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
  {
    /* Correct ordering here is crucial.  We must set
        cur->pagedir to NULL before switching page directories,
        so that a timer interrupt can't switch back to the
        process page directory.  We must activate the base page
        directory before destroying the process's page
        directory, or our active page directory will be one
        that's been freed (and cleared). */
    //!输出
    printf ("%s: exit(%d)\n",thread_name(), thread_current()->exitStatus);
    cur->pagedir = NULL;
    pagedir_activate (NULL);
    pagedir_destroy (pd);
  }
}

/* Sets up the CPU for running user code in the current
   thread. This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{

  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */

  file = filesys_open (file_name);
  if (file == NULL)
  {
    printf ("load: %s: open failed\n", file_name);
    goto done;
  }
  /* Deny write for the opened file by calling file deny write */
  file_deny_write(file);
  t->nowfile = file;
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }


  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
