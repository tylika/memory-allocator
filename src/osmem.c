// SPDX-License-Identifier: BSD-3-Clause

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "osmem.h"
#include "helpers.h"

/*
 * Allocate @size bytes, using mmap() when the block (metadata included)
 * reaches @threshold and the heap otherwise. Returns the payload address.
 */
static void *alloc_block(size_t size, size_t threshold)
{
	struct block_meta *block;

	if (size == 0)
		return NULL;

	size = ALIGN(size);

	if (META_SIZE + size >= threshold)
		block = alloc_mapped_block(size);
	else
		block = alloc_heap_block(size);

	return BLOCK_PAYLOAD(block);
}

void *os_malloc(size_t size)
{
	return alloc_block(size, MMAP_THRESHOLD);
}

void os_free(void *ptr)
{
	struct block_meta *block;

	if (!ptr)
		return;

	block = PAYLOAD_BLOCK(ptr);

	if (block->status == STATUS_MAPPED)
		free_mapped_block(block);
	else if (block->status == STATUS_ALLOC)
		block->status = STATUS_FREE; /* heap memory is kept for reuse */
}

void *os_calloc(size_t nmemb, size_t size)
{
	size_t total;
	void *ptr;

	if (nmemb == 0 || size == 0)
		return NULL;

	/* Refuse sizes that overflow size_t */
	if (nmemb > SIZE_MAX / size)
		return NULL;

	total = nmemb * size;
	ptr = alloc_block(total, (size_t)getpagesize());
	memset(ptr, 0, total);

	return ptr;
}

/* Try to resize a heap block without moving it. Returns 1 on success. */
static int resize_in_place(struct block_meta *block, size_t size)
{
	/* Shrinking: keep the block and give the rest back as a free block */
	if (size <= block->size) {
		split_block(block, size);
		return 1;
	}

	/* Absorb the following free blocks one at a time until it fits */
	while (block->next && block->next->status == STATUS_FREE && block->size < size)
		merge_with_next(block);

	if (block->size >= size) {
		split_block(block, size);
		return 1;
	}

	/* The last block on the heap can simply grow with brk() */
	if (!block->next) {
		grow_heap(size - block->size);
		block->size = size;
		return 1;
	}

	return 0;
}

void *os_realloc(void *ptr, size_t size)
{
	struct block_meta *block;
	void *new_ptr;

	if (!ptr)
		return os_malloc(size);

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	block = PAYLOAD_BLOCK(ptr);

	/* Reallocating a free block is undefined behaviour, refuse it */
	if (block->status == STATUS_FREE)
		return NULL;

	size = ALIGN(size);

	/* Heap blocks that stay below the mmap threshold are resized in place */
	if (block->status == STATUS_ALLOC && META_SIZE + size < MMAP_THRESHOLD &&
	    resize_in_place(block, size))
		return ptr;

	/* Otherwise move the data to a new block */
	new_ptr = os_malloc(size);
	memcpy(new_ptr, ptr, block->size < size ? block->size : size);
	os_free(ptr);

	return new_ptr;
}
