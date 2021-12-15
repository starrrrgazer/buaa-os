//
// Created by 王洛霖 on 2021/12/14.
//

#ifndef BUAA_OS_FRAME_H
#define BUAA_OS_FRAME_H

#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

/**/
struct list frame_table;



/*页框管理*/
struct frame_table_entry{
  /*对应的物理地址*/
  void *physical_address;       
  /*list中的元素*/
  struct list_elem elem;     
  /*对应的虚拟地址*/
  void *virtual_address; 
  /*对应的进程*/      
  struct thread *thread;
};

void vm_init_frame ();
void* vm_get_frame (enum palloc_flags flags);
void vm_free_frame (void *virtual_page);



#endif //BUAA_OS_FRAME_H


