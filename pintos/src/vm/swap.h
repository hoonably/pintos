#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include <stddef.h>

void swap_init(void);
bool swap_out(const void *kpage, size_t *slot);
bool swap_in(size_t slot, void *kpage);
void swap_free(size_t slot);

#endif