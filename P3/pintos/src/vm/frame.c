//
// Created by 王洛霖 on 2021/12/14.
//

#include "frame.h"
#include "lib/kernel/list.h"
#include <hash.h>
#include <list.h>

#include <stdio.h>
#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"

/* 全局的锁保证操作frametable时不会被中断 */
static struct lock mylock;

static struct hash frame_map;
static struct list frame_list;      /* 框架列表 */
static struct list_elem *clock; /* clock算法中的指针 */

/*msy 选择要退出的帧*/
static struct frame_table_entry *pick_frame_to_evict(uint32_t* pagedir);

static unsigned frame_hash(const struct hash_elem *elem, void *aux);
static bool     frame_less(const struct hash_elem *, const struct hash_elem *, void *aux);
struct frame_table_entry* next_clock(void);
/*同时在init.c加上宏定义*/
void vm_init_frame (){
  lock_init (&mylock);
  list_init(&frame_list);
  hash_init (&frame_map, frame_hash, frame_less, NULL);
  clock = NULL;
}
/*取消分配框架或页面（内部过程）,须上锁调用*/
  void vm_frame_free (void *virtual_page, bool page){

  ASSERT (lock_held_by_current_thread(&mylock) == true);
  ASSERT (is_kernel_vaddr(virtual_page));
  ASSERT (pg_ofs (virtual_page) == 0);

  struct frame_table_entry f;
  f.physical_address =virtual_page;

  struct hash_elem *he = hash_find (&frame_map, &(f.helem));
  if (he == NULL) {
    PANIC ("The page to be freed is not stored in the table");
  }
  struct frame_table_entry *ft_ptr;
  ft_ptr= hash_entry(he, struct frame_table_entry, helem);
  hash_delete (&frame_map, &ft_ptr->helem);
  list_remove (&ft_ptr->lelem);
  // struct list_elem *e;
  // e = list_head (&frame_table);
  // if(e==null){
  //   PANIC ("The page to be freed isn't in the table");
  // }
  // ft_ptr= list_entry(e, struct frame_table_entry, elem);
  if(page) palloc_free_page(virtual_page);
  
  free(ft_ptr);
  // while ((e = list_next (e)) != list_tail (&frame_table))
  //   {
  //     ft_ptr = list_entry (e, struct frame_table_entry, elem);
  //     if (ft_ptr->virtual_address == virtual_page)
  //       {
  //         list_remove (e);
  //         free (ft_ptr);
  //         break;
  //       }
  //   }
  // palloc_free_page(virtual_page);
  // lock_release (&mylock);
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
void* vm_get_frame (enum palloc_flags flags,void *upage){
  lock_acquire (&mylock);
  
  /*获取一个空闲页并返回其内核虚拟地址*/
  void *virtual_page= palloc_get_page (PAL_USER | flags);
  /*如果返回为空说明是用户池页面已满，需要交换机制*/
  if (virtual_page == NULL) {
    // //此处应该写一个交换机制
    // return NULL;
    /*首先，交换页面*/
    struct frame_table_entry *f=pick_frame_to_evict(thread_current()->pagedir);
    ASSERT(f->thread !=NULL&&f!=NULL);

    /*清除页面映射，并将其替换为swap*/
    ASSERT (f->thread->pagedir != (void*)0xcccccccc);/*msy 判断*/
    pagedir_clear_page(f->thread->pagedir,f->virtual_address);

    bool dirty = false;
    dirty = dirty || pagedir_is_dirty(f->thread->pagedir, f->virtual_address);
    dirty = dirty || pagedir_is_dirty(f->thread->pagedir, f->physical_address);

    uint32_t swap_index=vm_swap_out(f->physical_address);
    vm_supt_set_swap(f->thread->spt,f->virtual_address,swap_index);

    vm_spt_set_dirty(f->thread->spt, f->virtual_address, dirty);

    /*释放框架表并更新页面表*/
    vm_frame_free(f->physical_address,true);
    virtual_page=palloc_get_page(PAL_USER|flags);

    ASSERT(virtual_page!=NULL);
  }

  struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));

   if(frame == NULL) {
    // 框架分配失败 critical state or panic?
    return NULL;
  }

  frame->thread = thread_current ();
  frame->virtual_address = upage;
  //vtop返回内核虚拟地址所在的物理地址VADDR的映射。
  frame->physical_address = virtual_page;
  frame->unused_count=0;
  //插入list
  hash_insert (&frame_map, &frame->helem);
  list_push_back(&frame_table,&frame->lelem);
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
  vm_frame_free (virtual_page, true);
  lock_release (&mylock);
}

/*从entry中移除，但不释放*/
void vm_remove_frame (void *virtual_page)
{
  lock_acquire (&mylock);
  vm_frame_free (virtual_page,false);
  lock_release (&mylock);
}

/**页面驱逐策略*/
struct frame_table_entry * pick_frame_to_evict(uint32_t *pagedir)
{
	// struct list_elem * e;
	// struct frame_table_entry * victim_frame;
  // e=list_back(&frame_table);
  // victim_frame=list_entry(e,struct frame_table_entry, elem);
	// return victim_frame;

  size_t n = hash_size(&frame_table);
  if(n==0) PANIC("Frame table is empty");

  size_t i;
  for(i = 0; i<= n+n;i++) // 防止无限循环，2n次迭代就足够了
  {
    struct frame_table_entry *e = next_clock();
    // 如果unused_count==0,继续
    if(e->unused_count==0) continue;
    // if referenced, give a second chance.
    else if( pagedir_is_accessed(pagedir, e->virtual_address)) {
      pagedir_set_accessed(pagedir, e->virtual_address, false);
      continue;
      
    }
    //自从上次有机会以来没有被提及过
    return e;
  }
}

/*页面驱逐(pin)*/
void vm_count_frame (void* kpage,int mark)
{
  lock_acquire (&mylock);
  struct frame_table_entry victim_frame;
  victim_frame.physical_address = kpage;
  struct hash_elem *he = hash_find (&frame_map, &(victim_frame.helem));
  if (he == NULL) {
    PANIC ("The frame does not exist");
  }
  // struct list_elem * e;
  // // hash lookup : a temporary entry
  // struct frame_table_entry victim_frame;
  // victim_frame.physical_address = kpage;
  // e=list_back(&frame_table);
  // if (e == NULL) {
  //   printf("The frame does not exist");
  // }
  struct frame_table_entry *t;
  t = hash_entry(he, struct frame_table_entry, helem);
  t->unused_count = mark;

  lock_release (&mylock);
}
void
vm_frame_unpin (void* physical_page) {
  vm_count_frame (physical_page, 1);
}

void
vm_frame_pin (void* physical_page) {
  vm_count_frame (physical_page, 0);
}
unsigned frame_hash(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, helem);
  return hash_bytes( &entry->physical_address, sizeof entry->physical_address );
}
bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, helem);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, helem);
  return a_entry->physical_address < b_entry->physical_address;
}
struct frame_table_entry* next_clock(void)
{
  if (list_empty(&frame_list))
    PANIC("Frame table is empty");

  if (clock == list_end(&frame_list||clock == NULL ))
    clock = list_begin (&frame_list);
  else
    clock= list_next (clock);

  struct frame_table_entry *e = list_entry(clock, struct frame_table_entry, lelem);
  return e;
}




