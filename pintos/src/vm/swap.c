#include "vm/swap.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include <string.h>

static struct lock swap_lock;
static struct bitmap *swap_bitmap;
static struct block *swap_block;

// 한 페이지를 구성하는 섹터 개수 : 페이지 크기(PGSIZE) / 블록 섹터 크기(BLOCK_SECTOR_SIZE)
// PGSIZE는 4096 bytes, BLOCK_SECTOR_SIZE는 512 bytes
#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)  //! => 4096 / 512 = 8

void
swap_init(void) {
  lock_init(&swap_lock);
  swap_block = block_get_role(BLOCK_SWAP);
  if (swap_block == NULL)
    PANIC("swap_init: no swap block");
  size_t cnt = block_size(swap_block) / SECTORS_PER_PAGE;
  swap_bitmap = bitmap_create(cnt);
  if (swap_bitmap == NULL)
    PANIC("swap_init: bitmap_create failed");
}

/* kpage → 스왑 슬롯에 저장 */
bool
swap_out(const void *kpage, size_t *slot_) {
  lock_acquire(&swap_lock);
  size_t idx = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
  if (idx == BITMAP_ERROR) {
    lock_release(&swap_lock);
    return false;
  }
  size_t sector = idx * SECTORS_PER_PAGE;
  const char *buf = kpage;
  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; i++)
    block_write(swap_block, sector + i, buf + i * BLOCK_SECTOR_SIZE);
  lock_release(&swap_lock);
  *slot_ = idx;
  return true;
}

/* 스왑 슬롯 → kpage 로 복원 */
bool
swap_in(size_t slot, void *kpage) {
  if (slot == (size_t) -1 || !bitmap_test(swap_bitmap, slot))
    return false;
  lock_acquire(&swap_lock);
  size_t sector = slot * SECTORS_PER_PAGE;
  char *buf = kpage;
  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; i++)
    block_read(swap_block, sector + i, buf + i * BLOCK_SECTOR_SIZE);
  bitmap_reset(swap_bitmap, slot);
  lock_release(&swap_lock);
  return true;
}

/* 슬롯 해제 */
void
swap_free(size_t slot) {
  if (slot == (size_t)-1) return;
  lock_acquire(&swap_lock);
  bitmap_reset(swap_bitmap, slot);
  lock_release(&swap_lock);
}