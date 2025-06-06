#include "vm/frame.h"
#include "threads/malloc.h"  // malloc(), free()
#include "threads/synch.h"   // lock

/*
! 현재 frame table만 구현했고, 이 상태로 Project 2 테스트는 전부 통과함

TODO: Eviction (페이지 교체 정책)
- 현재는 물리 프레임이 부족하면 NULL을 반환
- 이후에는 eviction policy를 기반으로 victim frame을 선택하고 해당 페이지를 저장하고 해당 frame을 재할당
- page replacement 정책 = LRU 또는 Second-Chance
- accessed bit, dirty bit 이용

TODO: Swap 연동
- swap.c 구현 이후, evicted page는 swap 영역으로 저장 필요
- 저장된 페이지는 SPT(보조 페이지 테이블)를 통해 참조 가능해야 함
*/


static struct list frame_list;
static struct lock frame_lock;

void frame_table_init(void) {
    // 전역변수 초기화
    // 이 함수는 threads/init.c 에서 초기화!!!
    list_init(&frame_list);
    lock_init(&frame_lock);
}

// 새 프레임을 할당하고 frame_list에 등록

// flags에 들어오는 enum들
// PAL_USER: 사용자 프로세스를 위한 물리 페이지를 user pool에서 할당
// PAL_ZERO: 페이지를 0으로 초기화해서 할당
// PAL_ASSERT: 할당 실패 시 kernel panic
void *frame_alloc(enum palloc_flags flags, void *upage) {
    lock_acquire(&frame_lock);

    // 물리 페이지 하나 확보
    void *kpage = palloc_get_page(flags);
    if (kpage == NULL) {
        lock_release(&frame_lock);

        // TODO: frame이 부족할 경우 eviction 정책 적용
        // 1. frame_list에서 victim 선택 (accessed/dirty bit 기반)
        // 2. 해당 페이지를 swap 또는 파일에 저장
        // 3. 해당 frame을 재할당
        
        // 지금은 swap과 eviction 미구현 상태라 NULL 반환하고 그냥 끝
        return NULL;
    }

    // frame 메타데이터 동적 생성
    struct frame *f = malloc(sizeof(struct frame));
    if (f == NULL) {
        palloc_free_page(kpage);
        lock_release(&frame_lock);
        return NULL;
    }

    f->kpage = kpage;
    f->upage = upage;
    f->thread = thread_current();

    // frame 다 정리 됐으면 frame table에 등록
    list_push_back(&frame_list, &f->elem);

    lock_release(&frame_lock);
    return kpage;
}

// 등록된 프레임을 해제하고 frame_list에서 제거
void frame_free(void *kpage) {
    lock_acquire(&frame_lock);

    // frame_list에서 찾는 kpage 가진 frame 찾아서 해제
    struct list_elem *e;
    for (e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
        struct frame *f = list_entry(e, struct frame, elem);
        if (f->kpage == kpage) {
            // TODO: swap 연동시 swap slot 해제

            // TODO: supplemental page table 지우기


            list_remove(&f->elem);
            free(f);  // 메타데이터 해제
            palloc_free_page(kpage);
            break;
        }
    }

    lock_release(&frame_lock);
}
