//
// Created by 王洛霖 on 2021/12/14.
//

#include "page.h"

#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

/*v1.1添加了辅助页表管理（创建），并在thread里加了spt属性，在有pagedir创建和删除的地方加了相应的spt操作*/


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

struct supplemental_page_table_entry{
  /*对应的物理地址*/
  void *physical_address;       
  /*hash表中的元素*/
  struct hash_elem elem;     
  /*对应的虚拟地址*/
  void *virtual_address; 
  /*对应的进程*/      
  struct thread *thread;
};

unsigned page_hash(const struct hash_elem *p, void *aux) {
  struct supplemental_page_table_entry * entry = hash_entry(p, struct supplemental_page_table_entry, elem);
  return hash_bytes(&entry->virtual_address,sizeof entry->virtual_address);
}
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux) {
    struct supplemental_page_table_entry *a = hash_entry(a_, struct supplemental_page_table_entry, elem);
    struct supplemental_page_table_entry *b = hash_entry(b_, struct supplemental_page_table_entry, elem);
    return a->virtual_address < b->virtual_address;
}

struct supplemental_page_table* vm_create_spt (){
  struct supplemental_page_table *spt =(struct supplemental_page_table*) malloc(sizeof(struct supplemental_page_table));
  hash_init (&spt->pages, page_hash, page_less, NULL);
  return spt;
}


