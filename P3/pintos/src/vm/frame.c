//
// Created by 王洛霖 on 2021/12/14.
//

#include "frame.h"

/* v1.0
实现frametable的数据结构设计和基本管理（初始化，申请和释放）
根据官方文档，也修改了process.c中的load_segment，将其修改为我设计的页框管理函数 
*/

#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"


/* 全局的锁保证操作frametable时不会被中断 */
static struct lock mylock;

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

/*同时在init.c加上宏定义*/
void vm_init_frame (){
  lock_init (&mylock);
  list_init(&frame_table);
}


/*
palloc_get_page (enum palloc_flags flags) 
{
  return palloc_get_multiple (flags, 1);
}
获取一个空闲页并返回其内核虚拟地址。
   如果设置了PAL_USER，该页将从用户池中获得。
   否则从内核池中获取。 如果在FLAGS中设置了PAL_ZERO。
   那么该页将被填充为零。 如果没有页面
   则返回一个空指针，除非PAL_ASSERT被设置在
   FLAGS中设置了PAL_ASSERT，在这种情况下，内核会恐慌。
*/

/*在用户池中获取一个空闲页面并返回其内核虚拟地址。
此处相对于palloc_get_page函数增加了用户池已满时的交换机制和物理地址的映射*/
void* vm_get_frame (enum palloc_flags flags){
  lock_acquire (&mylock);
  
  /*获取一个空闲页并返回其内核虚拟地址*/
  void *virtual_page = palloc_get_page (PAL_USER | flags);
  /*如果返回为空说明是用户池页面已满，需要交换机制*/
  if (virtual_page == NULL) {
    //此处应该写一个交换机制
    return NULL;
  }

  struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));
  frame->thread = thread_current ();
  frame->virtual_address = virtual_page;
  //vtop返回内核虚拟地址所在的物理地址VADDR的映射。
  frame->physical_address = (void*) vtop(virtual_page);
  //插入list
  
  list_push_back(&frame_table,&frame->elem);
  lock_release (&mylock);

  return virtual_page;
}

/*
void
palloc_free_page (void *page) 
{
  palloc_free_multiple (page, 1);
}
释放页面.此处相对于palloc_free_page,多一个list remove释放资源
*/
void vm_free_frame (void *virtual_page){
  lock_acquire (&mylock);
  struct frame_table_entry *ft_ptr;
  struct list_elem *e;
  e = list_head (&frame_table);
  while ((e = list_next (e)) != list_tail (&frame_table))
    {
      ft_ptr = list_entry (e, struct frame_table_entry, elem);
      if (ft_ptr->virtual_address == virtual_page)
        {
          list_remove (e);
          free (ft_ptr);
          break;
        }
    }
  palloc_free_page(virtual_page);
  lock_release (&mylock);
}





