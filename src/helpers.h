/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <stddef.h>
#include "block_meta.h"

/* All payloads and metadata are aligned to 8 bytes */
#define ALIGNMENT		8
#define ALIGN(size)		(((size) + (ALIGNMENT - 1)) & ~((size_t)ALIGNMENT - 1))

/* Size of the metadata header placed in front of every payload */
#define META_SIZE		ALIGN(sizeof(struct block_meta))

/* Allocations at least this big (metadata included) are served with mmap() */
#define MMAP_THRESHOLD		(128 * 1024)

/* Size of the chunk requested from brk() the first time the heap is used */
#define HEAP_PREALLOC_SIZE	(128 * 1024)

/* Smallest block a split may produce: metadata + 1 byte (aligned) */
#define MIN_BLOCK_SIZE		(META_SIZE + ALIGN(1))

/* Conversions between a block header and the payload address returned to users */
#define BLOCK_PAYLOAD(block)	((void *)((char *)(block) + META_SIZE))
#define PAYLOAD_BLOCK(ptr)	((struct block_meta *)((char *)(ptr) - META_SIZE))

/* Allocate a new block with mmap() for a payload of @size (aligned) bytes */
struct block_meta *alloc_mapped_block(size_t size);

/* Release a block previously returned by alloc_mapped_block() */
void free_mapped_block(struct block_meta *block);

/*
 * Return a heap block with a payload of at least @size (aligned) bytes.
 * Preallocates the heap on first use, coalesces free blocks, reuses the
 * best fitting free block (splitting it if possible) and only then grows
 * the heap with brk(), expanding the last block if it is free.
 */
struct block_meta *alloc_heap_block(size_t size);

/* Split @block so its payload is exactly @size bytes, if the rest is usable */
void split_block(struct block_meta *block, size_t size);

/* Merge @block with the block that follows it on the heap */
void merge_with_next(struct block_meta *block);

/* Merge every pair of adjacent free blocks on the heap */
void coalesce_free_blocks(void);

/* Return the last block on the heap (NULL if the heap is not used yet) */
struct block_meta *get_last_block(void);

/* Grow the heap by @size bytes, returning the start of the new memory */
void *grow_heap(size_t size);
