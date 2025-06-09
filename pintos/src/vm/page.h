#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"

#define STACK_MAX_SIZE 0x800000  // 8 MB 스택 성장 크기

/*
page = 4096 bytes의 연속적인 virtual memory 영역
시작주소도 꼭 page size로 나누어 떨어져야 함

 	
    31               12 11        0
    +-------------------+-----------+
    |    Page Number    |   Offset  |
    +-------------------+-----------+
            Virtual Address

PHYS_BASE 0xc0000000 (3 GB) 아래에 User page가 생성됨


*/

// binary(text, data), file (mmap), 그 외 (stack, swap)
enum page_type { PAGE_BINARY, PAGE_MMAP, PAGE_STACK, PAGE_SWAP };

struct page {
    uint8_t *vaddr;  // fault된 가상 주소
    bool writable;  // 쓰기 가능한지
    bool is_loaded;  // frame에 올라갔는지
    enum page_type type;

    struct file *file;
    off_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    struct hash_elem elem;

    int swap_slot;
};

struct mmap_file {
    mapid_t idx;
    struct file *file;  // 매핑된 파일
    void *addr;  // 시작 가상 주소
    size_t length;  //* 전체 바이트 수
    struct list_elem elem;
};



unsigned page_hash_func(const struct hash_elem *e, void *aux UNUSED);
bool page_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
void page_table_init(struct hash *page_table);

bool allocate_page(enum page_type type, void *vaddr, bool writable);
bool load_page(struct page *vme);
struct page *find_page_entry(struct hash *page_table, void *vaddr);
bool insert_page_entry(struct hash *page_table, struct page *vme);


#endif