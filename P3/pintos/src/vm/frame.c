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

/* 根据官方文档，用一个hash表来分配物理页，其中的hash是定义在lib/kernel/hash.c */
/*下面是hash的结构
struct hash 
  {
    size_t elem_cnt;            
    size_t bucket_cnt;          
    struct list *buckets;       
    hash_hash_func *hash;       
    hash_less_func *less;      
    void *aux;                  
  };
*/
static struct hash hash_frame_table;
/*在hash表初始化函数中，需要定义两个函数
hash_init (struct hash *h,hash_hash_func *hash, hash_less_func *less, void *aux) 
其中
typedef unsigned hash_hash_func (const struct hash_elem *e, void *aux);计算并返回哈希元素E的哈希值，给定的是辅助数据AUX。
typedef bool hash_less_func (const struct hash_elem *a,
                             const struct hash_elem *b,
                             void *aux);比较两个哈希元素A和B的值，给定辅助数据AUX。
   如果A小于B，则返回真，如果A大于或等于B，则返回
   具体参考hash.h最上面的说明
*/
static unsigned my_hash_hash_func(const struct hash_elem *elem1, void *aux)
{
  struct frame_table *f = hash_entry(elem1, struct frame_table, elem);
  return hash_bytes( &f->physical_address, sizeof(f->physical_address)  );
}
static bool my_hash_less_func(const struct hash_elem *elem1, const struct hash_elem *elem2, void *aux)
{
  struct frame_table *f1 = hash_entry(elem1, struct frame_table, elem);
  struct frame_table *f2 = hash_entry(elem2, struct frame_table, elem);
  return f1->physical_address < f2->physical_address;
}

/*页框管理*/
struct frame_table{
  /*对应的物理地址*/
  void *physical_address;       
  /*hash表中的元素*/
  struct hash_elem elem;     
  /*对应的虚拟地址*/
  void *virtual_address; 
  /*对应的进程*/      
  struct thread *thread;
};

/*同时在init.c加上宏定义*/
void init_frame (){
  lock_init (&mylock);
  hash_init (&hash_frame_table, my_hash_hash_func, my_hash_less_func, NULL);
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
void* get_frame (enum palloc_flags flags){
  lock_acquire (&mylock);
  
  /*获取一个空闲页并返回其内核虚拟地址*/
  void *virtual_page = palloc_get_page (PAL_USER | flags);
  /*如果返回为空说明是用户池页面已满，需要交换机制*/
  if (virtual_page == NULL) {
    //此处应该写一个交换机制
    return NULL;
  }

  struct frame_table *frame = malloc(sizeof(struct frame_table));
  frame->thread = thread_current ();
  frame->virtual_address = virtual_page;
  //vtop返回内核虚拟地址所在的物理地址VADDR的映射。
  frame->physical_address = (void*) vtop(virtual_page);
  //插入hash表
  
  hash_insert (&hash_frame_table, &frame->elem);
  lock_release (&mylock);

  return virtual_page;
}

/*
void
palloc_free_page (void *page) 
{
  palloc_free_multiple (page, 1);
}
释放页面.此处相对于palloc_free_page,多一个hash表释放、frame_table释放的操作
*/
void free_frame (void *virtual_page){
  lock_acquire (&mylock);
  struct frame_table ft;
  ft.virtual_address=virtual_page;
  //根据virtualpage在hash表里找到对应的elem
  struct hash_elem *h = hash_find (&hash_frame_table, &(ft.elem));
  struct frame_table *f;
  //根据elem在hash表里找到对应的frame table * 
  f = hash_entry(h, struct frame_table, elem);
  hash_delete (&hash_frame_table, &f->elem);
  palloc_free_page (f->virtual_address);
  free(f);
  lock_release (&mylock);
}





