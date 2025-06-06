#ifndef VM_FRAME_H
#define VM_FRAME_H

/*
Frame : 연속적인 physical memory 영역
page랑 똑같이 4096 bytes
page-aligned 되어야함 (4096 배수로 시작 주소)

80x86 아키텍처는 물리 주소를 직접 접근하는 방법 제공 안함.
해결책 -> kernel virtual memory를 physical memory에 직접 매핑 
커널 가상 메모리의 첫 page = 물리 메모리의 첫 frame
-> 가상 주소를 통해 물리 주소에 간접적으로 접근
*/

#include "threads/palloc.h"  // enum palloc_flags
#include "threads/thread.h"  // struct thread
#include <list.h>  // struct list_elem

struct frame {
    void *kpage;  // 커널 가상 주소 (물리 메모리에 매핑되어있음)
    void *upage;  // 대응되는 유저 가상 주소
    struct thread *thread;  // 이 프레임을 가진 thread
    struct list_elem elem;
};

void *frame_alloc(enum palloc_flags flags, void *upage);
void frame_free(void *kpage);
void frame_table_init(void);

#endif
