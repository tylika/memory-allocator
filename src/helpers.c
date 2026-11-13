// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include "helpers.h"

/*
 * Doubly linked list of all the blocks placed on the heap, sorted by address.
 * Heap blocks are contiguous: every block starts right after the previous one.
 * Mapped blocks are not kept in this list, they are released with munmap().
 */
static struct block_meta *heap_start;

void *grow_heap(size_t size)
{
	void *ptr = sbrk(size);

	DIE(ptr == (void *)-1, "sbrk");

	return ptr;
}

struct block_meta *alloc_mapped_block(size_t size)
{
	struct block_meta *block;

	block = mmap(NULL, META_SIZE + size, PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	DIE(block == MAP_FAILED, "mmap");

	block->size = size;
	block->status = STATUS_MAPPED;
	block->prev = NULL;
	block->next = NULL;

	return block;
}

void free_mapped_block(struct block_meta *block)
{
	int ret = munmap(block, META_SIZE + block->size);

	DIE(ret == -1, "munmap");
}

struct block_meta *get_last_block(void)
{
	struct block_meta *block = heap_start;

	if (!block)
		return NULL;

	while (block->next)
		block = block->next;

	return block;
}

void split_block(struct block_meta *block, size_t size)
{
	struct block_meta *rest;

	/* The remainder must hold a header and at least one aligned byte */
	if (block->size < size + MIN_BLOCK_SIZE)
		return;

	rest = (struct block_meta *)((char *)block + META_SIZE + size);
	rest->size = block->size - size - META_SIZE;
	rest->status = STATUS_FREE;
	rest->prev = block;
	rest->next = block->next;

	if (block->next)
		block->next->prev = rest;

	block->next = rest;
	block->size = size;
}

void merge_with_next(struct block_meta *block)
{
	struct block_meta *next = block->next;

	block->size += META_SIZE + next->size;
	block->next = next->next;

	if (next->next)
		next->next->prev = block;
}

void coalesce_free_blocks(void)
{
	struct block_meta *block = heap_start;

	while (block && block->next) {
		if (block->status == STATUS_FREE && block->next->status == STATUS_FREE)
			merge_with_next(block); /* stay on block, it may absorb more */
		else
			block = block->next;
	}
}

/* Return the smallest free block that can hold @size bytes, or NULL */
static struct block_meta *find_best_block(size_t size)
{
	struct block_meta *best = NULL;

	for (struct block_meta *block = heap_start; block; block = block->next) {
		if (block->status != STATUS_FREE || block->size < size)
			continue;

		if (!best || block->size < best->size)
			best = block;
	}

	return best;
}

/* Request the first chunk of the heap as one big free block */
static void preallocate_heap(void)
{
	heap_start = grow_heap(HEAP_PREALLOC_SIZE);
	heap_start->size = HEAP_PREALLOC_SIZE - META_SIZE;
	heap_start->status = STATUS_FREE;
	heap_start->prev = NULL;
	heap_start->next = NULL;
}

struct block_meta *alloc_heap_block(size_t size)
{
	struct block_meta *block, *last;

	if (!heap_start)
		preallocate_heap();

	/* Reuse the best fitting free block */
	coalesce_free_blocks();
	block = find_best_block(size);
	if (block) {
		split_block(block, size);
		block->status = STATUS_ALLOC;
		return block;
	}

	/* No fit: expand the last block if it is free */
	last = get_last_block();
	if (last->status == STATUS_FREE) {
		grow_heap(size - last->size);
		last->size = size;
		last->status = STATUS_ALLOC;
		return last;
	}

	/* Otherwise place a brand new block at the end of the heap */
	block = grow_heap(META_SIZE + size);
	block->size = size;
	block->status = STATUS_ALLOC;
	block->prev = last;
	block->next = NULL;
	last->next = block;

	return block;
}
