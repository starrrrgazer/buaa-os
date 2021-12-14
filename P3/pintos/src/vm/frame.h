//
// Created by 王洛霖 on 2021/12/14.
//

#ifndef BUAA_OS_FRAME_H
#define BUAA_OS_FRAME_H

#endif //BUAA_OS_FRAME_H


#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

void vm_init_frame ();
void* vm_get_frame (enum palloc_flags flags);
void vm_free_frame (void *virtual_page);