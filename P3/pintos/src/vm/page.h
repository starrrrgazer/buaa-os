//
// Created by 王洛霖 on 2021/12/14.
//

#ifndef BUAA_OS_PAGE_H
#define BUAA_OS_PAGE_H

#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

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
/*辅助页表*/
struct supplemental_page_table {
	struct hash pages;
};
/*
根据官方文档， If the memory reference is valid, use the supplemental page table entry to locate the data that goes in the page, which might be in the file system, or in a swap slot, or it might simply be an all-zero page.
将page的状态分类为all-zero,frame, swap,filesys
*/
enum page_status {
  ALL_ZERO,         
  FRAME,         
  SWAP,
  FILESYS
};
struct supplemental_page_table_entry{
  /*对应的物理地址,kpage*/
  void *physical_address;       
  /*hash表中的元素*/
  struct hash_elem elem;     
  /*对应的虚拟地址,upage*/
  void *virtual_address;  
  /*对应的进程*/      
  struct thread *thread;
  /*page对应的状态*/
  enum page_status status;
};

struct supplemental_page_table* vm_create_spt ();
bool vm_spt_set_page (struct supplemental_page_table *spt, void *virtual_page);
bool vm_load_page(struct supplemental_page_table *spt, int *pagedir, void *virtual_page);



#endif //BUAA_OS_PAGE_H
