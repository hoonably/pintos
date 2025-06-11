#include "vm/frame.h"
#include "threads/malloc.h"  // malloc(), free()
#include "threads/synch.h"   // lock
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/page.h"

static struct list frame_list;
static struct lock frame_lock;

void *frame_evict_and_reuse(void) {
    struct list_elem *e;
    lock_acquire(&frame_lock);
    while(1) {
        for (e = list_begin(&frame_list);
             e != list_end(&frame_list);
             e = list_next(e)) {
            struct frame *victim = list_entry(e, struct frame, elem);
            if (!pagedir_is_accessed(victim->thread->pagedir, victim->upage)) {
                pagedir_clear_page(victim->thread->pagedir, victim->upage);
                size_t slot;
                struct page *vme = find_page_entry(
                        &victim->thread->page_table, victim->upage);
                
                //! vme가 NULL이 아닐때만 swap_out
                if (vme != NULL) {
                    swap_out(victim->kpage, &slot);
                    vme->swap_slot = (int)slot;
                    vme->type      = PAGE_SWAP;
                    vme->is_loaded = false;
                }
                void *k = victim->kpage;
                list_remove(&victim->elem);
                free(victim);
                lock_release(&frame_lock);
                return k;
            }
            pagedir_set_accessed(
                victim->thread->pagedir, victim->upage, false);
        }
    }
}

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
    // 물리 페이지 하나 확보
    void *kpage = palloc_get_page(flags);
    if (kpage == NULL) {
        kpage = palloc_get_page(0);
        if (kpage == NULL) {
            //! frame이 부족할 경우 eviction 정책 적용
            kpage = frame_evict_and_reuse();  // 안쓰는 변수라 그냥 뺌
            if (!kpage)
                return NULL;
        }
    }

    lock_acquire(&frame_lock);
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
    for (e = list_begin(&frame_list);
             e != list_end(&frame_list);
             e = list_next(e)) {
        struct frame *f = list_entry(e, struct frame, elem);
        if (f->kpage == kpage) {
            //! 보조 페이지 테이블에서 대응 vme 찾기
            struct page *vme = find_page_entry(&f->thread->page_table, f->upage);
            //! swap slot에 올라가 있으면 해제
            if (vme != NULL && vme->type == PAGE_SWAP && vme->swap_slot >= 0)
                swap_free((size_t)vme->swap_slot);

            list_remove(&f->elem);
            free(f);
            palloc_free_page(kpage);
            break;
        }
    }

    lock_release(&frame_lock);
}