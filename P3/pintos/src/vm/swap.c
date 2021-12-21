//
// Created by 王洛霖 on 2021/12/14.
//
#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "vm/swap.h"

static struct block *sblock;
static struct bitmap *sbitmap;

static const size_t SPAGE = PGSIZE / BLOCK_SECTOR_SIZE;

// the number of possible (swapped) pages.
static size_t ssize;
//!gb:初始化
void
vm_swap_init ()
{
    
  ASSERT (SPAGE > 0);
//!gb:初始化磁盘交换的表
  sblock = block_get_role(BLOCK_SWAP);
  if(sblock == NULL) {
    printf("vm_swap_init err");
    NOT_REACHED ();
  }

//!gb:初始化sbitmap，sbitmap的每一个位都对应一个块区域，该区域由连续的SPAGE扇区组成，它们的总大小等于PGSIZE。
  ssize = block_size(sblock) / SPAGE;
  sbitmap = bitmap_create(ssize);
  bitmap_set_all(sbitmap, true);
}

//!gb:将page内容写入交换表，返回交换区处的索引
uint32_t vm_swap_out (void *page)
{
  //!gb：检查此page是不是在虚拟内存
  ASSERT (page >= PHYS_BASE);
  size_t i;
  //!gb:扫描block区域寻找可以使用的地方
  size_t swap_idx = bitmap_scan (sbitmap, 0, 1, true);

  //!gb:存入
  for (i=0;i<SPAGE;i++) {
    block_write(sblock,swap_idx*SPAGE+i,page+BLOCK_SECTOR_SIZE*i);
  }
  bitmap_set(sbitmap, swap_idx, false);
  return swap_idx;
}

//!gb:将swap内容写入page
void vm_swap_in (void *page, uint32_t swap_idx)
{
  //!gb：检查此page是不是在虚拟内存
  ASSERT (page >= PHYS_BASE);
  //!gb:检查交换表
  ASSERT (swap_idx < ssize);
  if (bitmap_test(sbitmap, swap_idx) == true) {
    printf("vm_swap_in err");
  }
  //!gb:读出
  size_t i;
  for (i=0;i<SPAGE;i++) {
    block_read (sblock,swap_idx*SPAGE+i,page+BLOCK_SECTOR_SIZE*i);
  }

  bitmap_set(sbitmap, swap_idx, true);
}

/*检查交换区域*/
void vm_swap_free (uint32_t swap_idx)
{
  //检查交换区域
  ASSERT (swap_idx < ssize);
  if (bitmap_test(sbitmap, swap_idx) == true) {
    PANIC ("Error, invalid free request to unassigned swap block");
  }
  bitmap_set(sbitmap, swap_idx, true);
}