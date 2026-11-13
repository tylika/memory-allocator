// SPDX-License-Identifier: BSD-3-Clause

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
	/* TODO: Implement os_calloc */
	return NULL;
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	return NULL;
}
