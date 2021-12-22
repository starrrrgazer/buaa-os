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




/*页框管理*/
struct frame_table_entry{
  /*对应的物理地址*/
  void *physical_address;       
  /*list中的元素*/
  // struct list_elem elem; 
  struct hash_elem helem;  
  struct list_elem lelem;    
  /*对应的虚拟地址*/
  void *virtual_address; 
  /*对应的进程*/      
  struct thread *thread;
  /*用于防止页面在获取某些资源时被逐出。若为0，则不会被驱逐*/
  int unused_count;
};

void vm_init_frame ();
void* vm_get_frame (enum palloc_flags flags,void *upage);
void vm_free_frame (void *virtual_page);
void vm_count_frame(void* kpage,int mark);
void vm_frame_free(void *virtual_page,bool page);
void vm_remove_frame (void *virtual_page);
void vm_frame_pin (void* physical_page);
void vm_frame_unpin (void* physical_page);



#endif //BUAA_OS_FRAME_H


