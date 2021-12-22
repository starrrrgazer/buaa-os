//
// Created by 王洛霖 on 2021/12/14.
//

#ifndef BUAA_OS_PAGE_H
#define BUAA_OS_PAGE_H

#include "lib/kernel/hash.h"
#include "vm/swap.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"

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
uint32_t swap_index;
enum page_status {
    ALL_ZERO,
    FRAME,
    SWAP,
    FROM_FILESYS
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
    bool dirty; //脏位
    uint32_t swap_index;
    struct file *file;
    off_t file_offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
};

struct supplemental_page_table* vm_create_spt ();
bool vm_spt_frame_install (struct supplemental_page_table *spt, void *virtual_page, void *physical_page);
bool vm_load_page(struct supplemental_page_table *spt, uint32_t *pagedir, void *virtual_page);
bool vm_supt_set_swap (struct supplemental_page_table *supt, void *, uint32_t);
bool vm_spt_has_entry (struct supplemental_page_table *, void *page);
bool vm_spt_filesys_install (struct supplemental_page_table *spt, void *upage,struct file * file,
                             off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
struct supplemental_page_table_entry* vm_spt_lookup (struct supplemental_page_table *supt, void *);
bool vm_spt_unmap(struct supplemental_page_table *spt,uint32_t *pagedir,void *page, struct file *file, off_t offset, size_t bytes);
void vm_pin_page(struct supplemental_page_table *spt, void *page);
void vm_unpin_page(struct supplemental_page_table *spt, void *page);
bool vm_spt_set_dirty (struct supplemental_page_table *spt, void *, bool);


#endif //BUAA_OS_PAGE_H
