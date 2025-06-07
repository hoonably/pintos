#include "vm/page.h"

/*
! Supplemental Page Table
: CPU가 virtual 주소를 physical 주소로 변환할 때 사용하는 데이터 구조
page => frame 매핑

* userprog/pagedir.c 에 구현된 함수를 직접 사용해야함

                          +----------+
         .--------------->|Page Table|---------.
        /                 +----------+          |
   31   |   12 11    0                    31    V   12 11    0
  +-----------+-------+                  +------------+-------+
  |  Page Nr  |  Ofs  |                  |  Frame Nr  |  Ofs  |
  +-----------+-------+                  +------------+-------+
   Virt Addr      |                       Phys Addr       ^
                   \_____________________________________/


page number가 대응되는 frame number로 변환됨
offset은 변하지 않음

중요 역할
1. page fault 발생시 그 페이지가 어떤 데이터를 가져와야 하는지 찾아내기 위해 커널이 page table을 사용함
userprog/exception.c의 page_fault() 함수 수정
- 접근한 가상 주소에 대한 page table entry를 찾음
    - entry는 해당 페이지에 어떤 데이터를 넣어야 할지 알려줌
    - file system, swap slot, 전부 0인 페이지 등 모두 가능. 이미 다른 frame에서 로드되었다면 그 frame을 재사용
- 접근 자체가 유효한지 검증 -> 유효하지 않다면 process exit + 자원 모두 해제
    유효하지 않은 경우
    - page table enyry가 없음
    - kernel virtual memory에 user process가 접근하려고 함
    - read-only 페이지에 write 시도
    - page table이 해당 주소에 어떤 데이터도 있어야 한다고 표시하지 않은 경우
- 페이지를 담을 frame 확보
- 확보한 frame에 데이터 채우기
    - file system에서 읽어오기
    - swap slot 복원
    - 0으로 초기화
- page table 갱신 : 가상 주소에 대응되는 page table entry 물리 주소로 업데이트 (userprog/pagedir.c 함수 이용)

2. 프로세스가 종료될 때 어떤 리소스 (file, frame, swap slot 등)을 해제해야 하는지 결정하기 위해 사용됨
    - process_exit()에서 page table을 돌면서 해당 페이지에 매핑된 리소스를 해제
    - file이 매핑된 경우 file_close()
    - frame이 매핑된 경우 frame_free()
    - swap slot이 매핑된 경우 swap_free()

두가지 방식 사용 가능
// segment 기반 : stack, code, file 영역 등 범위 단위 구성 -> 복잡할듯 그냥 페이지 기반으로
page 기반 : 개별 가상 페이지 단위로 구성



*/

unsigned vm_hash_func(const struct hash_elem *e, void *aux UNUSED) {
    struct page *vme = hash_entry(e, struct page, elem);
    return hash_bytes(&vme->vaddr, sizeof(vme->vaddr));
}

bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct page *vma = hash_entry(a, struct page, elem);
    struct page *vmb = hash_entry(b, struct page, elem);
    return vma->vaddr < vmb->vaddr;
}

void page_table_init(struct hash *page_table) {
    hash_init(page_table, vm_hash_func, vm_less_func, NULL);
}

bool insert_page_entry(struct hash *page_table, struct page *vme) {
    if (hash_insert(page_table, &vme->elem) == NULL)
        return true;
    else
        return false;  // 중복일 경우
}

struct page *find_page_entry(struct hash *page_table, void *vaddr) {
    struct page tmp;
    tmp.vaddr = pg_round_down(vaddr);  // 페이지 단위 4096 bytes 배수로 내림
    struct hash_elem *e = hash_find(page_table, &tmp.elem);  // 해시 테이블에서 vaddr 페이지 단위 찾기
    if (e == NULL) return NULL;
    return hash_entry(e, struct page, elem);
}

bool allocate_page(enum page_type type, void *vaddr, bool writable) {
    struct page *vme = malloc(sizeof(struct page));
    if (!vme) return false;
    
    vme->vaddr = pg_round_down(vaddr);
    vme->writable = writable;
    vme->is_loaded = false;
    vme->type = type;

    // file, offset 등은 이후 load_segment에서 설정

    return insert_page_entry(&thread_current()->page_table, vme);
}

bool load_page(struct page *vme) {
    ASSERT(vme != NULL);

    // 이미 로딩되어 있다면 성공 처리
    if (vme->is_loaded)
        return true;

    // 1. frame 할당
    uint8_t *kpage = frame_alloc(PAL_USER, vme->vaddr);
    if (kpage == NULL)
        return false;

    // 2. backing store에서 읽기
    if (vme->type == PAGE_MMAP || vme->type == PAGE_BINARY) {
        if (file_read_at(vme->file, kpage, vme->read_bytes, vme->offset) != (int)vme->read_bytes) {
            frame_free(kpage);
            return false;
        }
        memset(kpage + vme->read_bytes, 0, vme->zero_bytes);
    }
    else if (vme->type == PAGE_STACK) {
        // 초기 스택 페이지는 zeroed
        memset(kpage, 0, PGSIZE);
    }

    // 3. page table에 매핑
    struct thread *t = thread_current();
    if (!pagedir_set_page(t->pagedir, vme->vaddr, kpage, vme->writable)) {
        frame_free(kpage);
        return false;
    }

    vme->is_loaded = true;
    return true;
}
