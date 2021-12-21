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
#include "string.h"
#include "filesys/file.h"
static bool vm_load_from_filesys(struct supplemental_page_table_entry *, void *);

unsigned page_hash(const struct hash_elem *p, void *aux) {
  struct supplemental_page_table_entry * entry = hash_entry(p, struct supplemental_page_table_entry, elem);
  return hash_bytes(&entry->virtual_address,sizeof entry->virtual_address);
}
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux) {
    struct supplemental_page_table_entry *a = hash_entry(a_, struct supplemental_page_table_entry, elem);
    struct supplemental_page_table_entry *b = hash_entry(b_, struct supplemental_page_table_entry, elem);
    return a->virtual_address < b->virtual_address;
}
static void spte_destroy(struct hash_elem *elem, void *aux UNUSED)
{
  struct supplemental_page_table_entry *entry = hash_entry(elem, struct supplemental_page_table_entry, elem);

  // Clean up the associated frame
  if (entry->physical_address != NULL) {
    ASSERT (entry->status == FRAME);
    vm_remove_frame (entry->physical_address);
  }
  else if(entry->status == SWAP) {
    vm_swap_free (entry->swap_index);
  }

  // Clean up SPTE entry.
  free (entry);
}

struct supplemental_page_table* vm_create_spt(){
  struct supplemental_page_table *spt =(struct supplemental_page_table*) malloc(sizeof(struct supplemental_page_table));
  hash_init (&spt->pages, page_hash, page_less, NULL);
  return spt;
}
bool vm_supt_set_swap(struct supplemental_page_table *supt, void *page, uint32_t swap_idx){
  struct supplemental_page_table_entry *spte;
  spte = vm_spt_lookup(supt, page);
//  printf("SPTE %u\n", spte);
  if(spte == NULL) return false;

  spte->status = SWAP;
  spte->swap_index = swap_idx;
  return true;
}
/*载入一个页面*/
bool vm_spt_frame_install (struct supplemental_page_table *spt, void *virtual_page, void *physical_page){
  struct supplemental_page_table_entry *spte;
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));
  spte->virtual_address = virtual_page;
  spte->physical_address=physical_page;
  spte->dirty = false;
  spte->status = FRAME;
  spte->swap_index=-1;
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
/*销毁释放一个spt*/
void
vm_spt_destroy (struct supplemental_page_table *spt)
{
  ASSERT (spt!=NULL);
  hash_destroy (&spt->pages, spte_destroy);
  free (spt);

  // struct supplemental_page_table_entry *e = hash_entry(e, struct supplemental_page_table_entry, elem);

  // //清除关联框架
  // if (e->physical_address != NULL) {
  //   ASSERT (e->status == FRAME);
  //   vm_remove_frame (e->physical_address);
  // }

  // // 清除SPTE的entry.
  // free (e);
}
bool
vm_spt_filesys_install (struct supplemental_page_table *spt, void *upage,struct file * file, 
                        off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  struct supplemental_page_table_entry *t;
  t = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));

  t->virtual_address = upage;
  t->physical_address=NULL;
  t->dirty = false;
  t->status = FROM_FILESYS;
  t->file = file;
  t->file_offset = offset;
  t->read_bytes = read_bytes;
  t->zero_bytes = zero_bytes;
  t->writable = writable;

  struct hash_elem *old;
  old = hash_insert (&spt->pages, &t->elem);
  if (old == NULL) return true;
  else
  {
    return false;
  }
}

//!P3:
//!gb:
bool vm_spt_zeropage (struct supplemental_page_table *spt, void *virtual_page){
  struct supplemental_page_table_entry *spte;
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));
  spte->virtual_address = virtual_page;
  spte->physical_address=NULL;
  spte->dirty = false;
  spte->status = ALL_ZERO;
  struct hash_elem *old;
  old = hash_insert (&spt->pages, &spte->elem);
  if (old == NULL) return true;
  else
  {
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
bool
vm_spt_has_entry (struct supplemental_page_table *supt, void *page)
{
  /* Find the SUPT entry. If not found, it is an unmanaged page. */
  struct supplemental_page_table_entry *spte = vm_spt_lookup(supt, page);
  if(spte == NULL) return false;
  return true;
}

/*将由地址virtual_page指定的页面装回内存中。*/
bool vm_load_page(struct supplemental_page_table *spt, int *pagedir, void *virtual_page){
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
  void *frame_page = vm_get_frame(PAL_USER,virtual_page);
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
    vm_swap_in (spte->swap_index, frame_page);
    break;

  case FROM_FILESYS:
    //需要一个文件系统相关的机制
    if( vm_load_from_filesys(spte, frame_page) == false) {
      vm_free_frame(frame_page);
      return false;
    }

    writable = spte->writable;
    break;
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

static bool vm_load_from_filesys(struct supplemental_page_table_entry *spte, void *kpage)
{
  file_seek (spte->file, spte->file_offset);

  // 从文件中读取字节
  int num = file_read (spte->file, kpage, spte->read_bytes);
  if(num!= (int)spte->read_bytes)
    return false;

  // 剩余字节为0
  ASSERT (spte->read_bytes + spte->zero_bytes == PGSIZE);
  memset (kpage + num, 0, spte->zero_bytes);
  return true;
}
/*msy update*/
bool vm_spt_unmap(struct supplemental_page_table *spt,uint32_t *pagedir,void *page, struct file *file, off_t offset, size_t bytes)
{
  file_seek(file, offset);
  struct supplemental_page_table_entry *spte = vm_spt_lookup(spt, page);
  if(spte == NULL) {
    PANIC ("munmap - some page is missing");
  }
  
  switch (spte->status)
  {
    case FRAME:
      // TODO dirty frame handling (write into file)
    ASSERT (spte->physical_address != NULL);

    // 脏框架处理（写入文件）
    // 检查虚拟页面或框架映射是否脏。如果是，则写入文件。
    bool dirty = spte->dirty;
    dirty = dirty || pagedir_is_dirty(pagedir, spte->virtual_address)||pagedir_is_dirty(pagedir, spte->physical_address);
    if(dirty) {
      file_write_at (file, spte->virtual_address, PGSIZE, offset);
    }
    //清除页面映射，然后释放框架
    // ASSERT (spte->physical_address != NULL);
    vm_free_frame (spte->physical_address);
    pagedir_clear_page (pagedir, spte->virtual_address);
    break;
    vm_frame_pin (spte->physical_address);

    case SWAP:
      {
      bool dirty = spte->dirty;
      dirty = dirty || pagedir_is_dirty(pagedir, spte->physical_address);
      if (dirty) {
        // load from swap, and write back to file
        void *tmp = palloc_get_page(0); // in the kernel
        vm_swap_in (spte->swap_index, tmp);
        file_write_at (file, tmp, bytes, offset);
        palloc_free_page(tmp);
      }
      else {
        // just throw away the swap.
        vm_swap_free (spte->swap_index);
      }
    }
      break;

    case FROM_FILESYS:
      // do nothing.
      break;

    default:
      PANIC ("default");
  }

  // the supplemental page table entry is also removed.
  // so that the unmapped memory is unreachable. Later access will fault.
  hash_delete(& spt->pages, &spte->elem);
  return true;
}
void
vm_pin_page(struct supplemental_page_table *spt, void *page)
{
  struct supplemental_page_table_entry *spte;
  spte = vm_spt_lookup(spt, page);
  if(spte == NULL) {
    // ignore. stack may be grow
    return;
  }

  ASSERT (spte->status == FRAME);
  vm_frame_pin (spte->physical_address);
}

void
vm_unpin_page(struct supplemental_page_table *spt, void *page)
{
  struct supplemental_page_table_entry *spte;
  spte = vm_spt_lookup(spt, page);
  if(spte == NULL) PANIC ("request page is non-existent");

  if (spte->status == FRAME) {
    vm_frame_unpin (spte->physical_address);
  }
}
bool
vm_spt_set_dirty (struct supplemental_page_table *spt, void *p, bool mark)
{
  struct supplemental_page_table_entry *spte = vm_spt_lookup(spt, p);
  if (spte == NULL) PANIC("set dirty - the request page doesn't exist");

  spte->dirty = spte->dirty ||mark;
  return true;
}