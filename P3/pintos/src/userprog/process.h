#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

typedef int mmapid_t;

struct mmap_desc {
  mmapid_t id;
  struct list_elem elem;
  struct file* file;

  void *addr;   // 存储virtual address
  size_t size;  // filesize
};

#endif /* userprog/process.h */
