//
// Created by 王洛霖 on 2021/12/14.
//

#include "page.h"

#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"


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

/*载入一个页面*/
bool vm_spt_set_page (struct supplemental_page_table *spt, void *virtual_page){
  struct supplemental_page_table_entry *spte;
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));
  spte->virtual_address = virtual_page;
  spte->status = FRAME;
  /*将NEW插入哈希表H中，如果表中没有相同的元素，则返回一个空指针。*/
  struct hash_elem *old;
  old = hash_insert (&spt->pages, &spte->elem);
  if (old == NULL) {
    return true;
  }
  else {
    free (spte);
    return false;
  }
}

struct supplemental_page_table_entry* vm_spt_lookup (struct supplemental_page_table *spt, void *page)
{
  // create a temporary object, just for looking up the hash table.
  struct supplemental_page_table_entry spte_temp;
  spte_temp.virtual_address = page;

  struct hash_elem *elem1 = hash_find (&spt->pages, &spte_temp.elem);
  if(elem1 == NULL) return NULL;
  return hash_entry(elem1, struct supplemental_page_table_entry, elem);
}


/*将由地址virtual_page指定的页面装回内存中。*/
bool vm_load_page(struct supplemental_page_table *spt, uint32_t *pagedir, void *virtual_page){
  /* see also userprog/exception.c */

  //先获取辅助页表
  struct supplemental_page_table_entry *spte;
  spte = vm_spt_lookup(spt, virtual_page);
  if(spte == NULL) {
    return false;
  }

  //已经载入,直接返回true
  if(spte->status == FRAME) {
    return true;
  }

  // 
  void *frame_page = vm_get_frame(PAL_USER);
  if(frame_page == NULL) {
    return false;
  }

  //将数据存入frame
  bool writable = true;//用于pagedir_set_page
  switch (spte->status)
  {
  case ALL_ZERO:
    memset (frame_page, 0, PGSIZE);
    break;

  case FRAME:
    break;

  case SWAP:
    // 需要一个交换机制
    break;

  case FROM_FILESYS:
    //需要一个文件系统相关的机制
    break;

  default:
    break;
  }

  // 将发生故障的虚拟地址的页表条目指向物理页。
  if(!pagedir_set_page (pagedir, virtual_page, frame_page, writable)) {
    vm_free_frame(frame_page);
    return false;
  }

  spte->physical_address = frame_page;
  spte->status = FRAME;

  return true;
}

