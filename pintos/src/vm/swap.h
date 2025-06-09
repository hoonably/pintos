#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include <stddef.h>

/* 스왑 슬롯 초기화 (bitmap, block device 준비) */
void swap_init(void);
/* kpage 에 담긴 프레임을 스왑 슬롯에 내보내고, 할당된 슬롯 번호를 *slot 에 반환 */
bool swap_out(const void *kpage, size_t *slot);
/* 스왑 슬롯 slot 에서 읽어 kpage 에 복원 */
bool swap_in(size_t slot, void *kpage);
/* 스왑 슬롯 slot 의 비트맵을 해제 */
void swap_free(size_t slot);

#endif