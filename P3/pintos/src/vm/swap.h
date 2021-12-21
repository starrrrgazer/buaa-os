//
// Created by 王洛霖 on 2021/12/14.
//

#ifndef BUAA_OS_SWAP_H
#define BUAA_OS_SWAP_H

//!gb:初始化
void vm_swap_init (void);


//!gb:将page内容写入交换表，返回交换区处的索引
uint32_t vm_swap_out (void *page);

//!gb:将swap内容写入page
void vm_swap_in (void *page,uint32_t swap_idx );
//msy 删除交换区域
void vm_swap_free (uint32_t swap_idx);

#endif /* vm/swap.h */
