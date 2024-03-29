#include "threads/malloc.h"
#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* A simple implementation of malloc().

   The size of each request, in bytes, is rounded up to a power
   of 2 and assigned to the "descriptor" that manages blocks of
   that size.  The descriptor keeps a list of free blocks.  If
   the free list is nonempty, one of its blocks is used to
   satisfy the request.

   Otherwise, a new page of memory, called an "arena", is
   obtained from the page allocator (if none is available,
   malloc() returns a null pointer).  The new arena is divided
   into blocks, all of which are added to the descriptor's free
   list.  Then we return one of the new blocks.

   When we free a block, we add it to its descriptor's free list.
   But if the arena that the block was in now has no in-use
   blocks, we remove all of the arena's blocks from the free list
   and give the arena back to the page allocator.

   We can't handle blocks bigger than 2 kB using this scheme,
   because they're too big to fit in a single page with a
   descriptor.  We handle those by allocating contiguous pages
   with the page allocator and sticking the allocation size at
   the beginning of the allocated block's arena header. */

/* Descriptor. */
struct desc
{
	size_t block_size;		 /* Size of each element in bytes. */
	size_t blocks_per_arena; /* Number of blocks in an arena. */
	struct list free_list;	 /* List of free blocks. */
	struct lock lock;		 /* Lock. */
};

/* Magic number for detecting arena corruption. */
#define ARENA_MAGIC 0x9a548eed

/* Arena. */
struct arena
{
	unsigned magic;	   /* Always set to ARENA_MAGIC. */
	struct desc *desc; /* Owning descriptor, null for big block. */
	size_t free_cnt;   /* Free blocks; pages in big block. */
};

/* Free block. */
struct block
{
	struct list_elem free_elem; /* Free list element. */
};

/* Our set of descriptors. */
static struct desc descs[10]; /* Descriptors. */
static size_t desc_cnt;		  /* Number of descriptors. */

static struct arena *block_to_arena(struct block *);
static struct block *arena_to_block(struct arena *, size_t idx);

/* Initializes the malloc() descriptors. */
void malloc_init(void)
{
	size_t block_size;

	for (block_size = 16; block_size < PGSIZE / 2; block_size *= 2)
	{
		struct desc *d = &descs[desc_cnt++];
		ASSERT(desc_cnt <= sizeof descs / sizeof *descs);
		d->block_size = block_size;
		d->blocks_per_arena = (PGSIZE - sizeof(struct arena)) / block_size;
		list_init(&d->free_list);
		lock_init(&d->lock);
	}
}

/* Obtains and returns a new block of at least SIZE bytes.
   Returns a null pointer if memory is not available. */
/* 최소 SIZE 바이트의 새 블록을 가져와 반환합니다.
   메모리를 사용할 수 없는 경우 널 포인터를 반환합니다. */
void *malloc(size_t size)
{
	struct desc *d;
	struct block *b;
	struct arena *a;

	/* A null pointer satisfies a request for 0 bytes. */
	/* 널 포인터는 0바이트에 대한 요청을 만족시킵니다. */
	if (size == 0)
		return NULL;

	/* Find the smallest descriptor that satisfies a SIZE-byte
	   request. */
	/* SIZE-byte 요청을 만족하는 가장 작은 디스크립터를 찾습니다. */
	for (d = descs; d < descs + desc_cnt; d++)
		if (d->block_size >= size)
			break;
	if (d == descs + desc_cnt)
	{
		/* SIZE is too big for any descriptor.
		   Allocate enough pages to hold SIZE plus an arena. */
		/* SIZE는 설명자로 사용하기에 너무 큽니다. SIZE에
		   아레나를 더할 수 있는 충분한 페이지를 할당하세요. */
		size_t page_cnt = DIV_ROUND_UP(size + sizeof *a, PGSIZE);
		a = palloc_get_multiple(0, page_cnt);
		if (a == NULL)
			return NULL;

		/* Initialize the arena to indicate a big block of PAGE_CNT
		   pages, and return it. */
		/* 아레나를 초기화하여 PAGE_CNT 페이지의 큰 블록을
		   표시하고 반환합니다. */
		a->magic = ARENA_MAGIC;
		a->desc = NULL;
		a->free_cnt = page_cnt;
		return a + 1;
	}

	lock_acquire(&d->lock);

	/* If the free list is empty, create a new arena. */
	/* 가용 목록이 비어 있으면 새 아레나를 만듭니다. */
	if (list_empty(&d->free_list))
	{
		size_t i;

		/* Allocate a page. */
		/* 페이지를 할당합니다. */
		a = palloc_get_page(0);
		if (a == NULL)
		{
			lock_release(&d->lock);
			return NULL;
		}

		/* Initialize arena and add its blocks to the free list. */
		/* 아레나를 초기화하고 해당 블록을 가용 목록에 추가합니다. */
		a->magic = ARENA_MAGIC;
		a->desc = d;
		a->free_cnt = d->blocks_per_arena;
		for (i = 0; i < d->blocks_per_arena; i++)
		{
			struct block *b = arena_to_block(a, i);
			list_push_back(&d->free_list, &b->free_elem);
		}
	}

	/* Get a block from free list and return it. */
	/* 가용 목록에서 블록을 가져와서 반환합니다. */
	b = list_entry(list_pop_front(&d->free_list), struct block, free_elem);
	a = block_to_arena(b);
	a->free_cnt--;
	lock_release(&d->lock);
	return b;
}

/* Allocates and return A times B bytes initialized to zeroes.
   Returns a null pointer if memory is not available. */
/* 0으로 초기화된 A 바이트와 B 바이트를 할당하고 반환합니다.
   메모리를 사용할 수 없는 경우 널 포인터를 반환합니다. */
void *calloc(size_t a, size_t b)
{
	/* Calculate block size and make sure it fits in size_t. */
	/* 블록 크기를 계산하고 size_t에 맞는지 확인합니다. */
	size_t size = a * b;

	if (size < a || size < b)
	{
		return NULL;
	}

	/* Allocate and zero memory. */
	/* 메모리를 할당하고 제로화합니다. */
	void *p = malloc(size);

	if (p != NULL)
	{
		memset(p, 0, size);
	}

	return p;
}

/* Returns the number of bytes allocated for BLOCK. */
static size_t
block_size(void *block)
{
	struct block *b = block;
	struct arena *a = block_to_arena(b);
	struct desc *d = a->desc;

	return d != NULL ? d->block_size : PGSIZE * a->free_cnt - pg_ofs(block);
}

/* Attempts to resize OLD_BLOCK to NEW_SIZE bytes, possibly
   moving it in the process.
   If successful, returns the new block; on failure, returns a
   null pointer.
   A call with null OLD_BLOCK is equivalent to malloc(NEW_SIZE).
   A call with zero NEW_SIZE is equivalent to free(OLD_BLOCK). */
void *
realloc(void *old_block, size_t new_size)
{
	if (new_size == 0)
	{
		free(old_block);
		return NULL;
	}
	else
	{
		void *new_block = malloc(new_size);
		if (old_block != NULL && new_block != NULL)
		{
			size_t old_size = block_size(old_block);
			size_t min_size = new_size < old_size ? new_size : old_size;
			memcpy(new_block, old_block, min_size);
			free(old_block);
		}
		return new_block;
	}
}

/* Frees block P, which must have been previously allocated with
   malloc(), calloc(), or realloc(). */
/* 이전에 malloc(), calloc() 또는 realloc()으로 할당되어야 했던
   블록 P를 해제합니다. */
void free(void *p)
{
	if (p != NULL)
	{
		struct block *b = p;
		struct arena *a = block_to_arena(b);
		struct desc *d = a->desc;

		if (d != NULL)
		{
			/* It's a normal block.  We handle it here. */
			/* 일반 블록입니다.  여기서 처리합니다. */

#ifndef NDEBUG
			/* Clear the block to help detect use-after-free bugs. */
			/* dangling pointer 버그를 탐지하는 데 도움이 되도록 block을 지웁니다. */
			memset(b, 0xcc, d->block_size);
#endif

			lock_acquire(&d->lock);

			/* Add block to free list. */
			/* 가용 목록에 블록을 추가합니다. */
			list_push_front(&d->free_list, &b->free_elem);

			/* If the arena is now entirely unused, free it. */
			/* 현재 아레나가 완전히 사용되지 않는다면 해제합니다. */
			if (++a->free_cnt >= d->blocks_per_arena)
			{
				size_t i;

				ASSERT(a->free_cnt == d->blocks_per_arena);
				for (i = 0; i < d->blocks_per_arena; i++)
				{
					struct block *b = arena_to_block(a, i);
					list_remove(&b->free_elem);
				}
				palloc_free_page(a);
			}

			lock_release(&d->lock);
		}
		else
		{
			/* It's a big block.  Free its pages. */
			/* 큰 블록입니다. 해당 페이지를 해제하세요. */
			palloc_free_multiple(a, a->free_cnt);
			return;
		}
	}
}

/* Returns the arena that block B is inside. */
/* 블록 B가 있는 아레나를 반환합니다. */
static struct arena *block_to_arena(struct block *b)
{
	struct arena *a = pg_round_down(b);

	/* Check that the arena is valid. */
	/* arena가 유효한지 확인합니다. */
	ASSERT(a != NULL);
	ASSERT(a->magic == ARENA_MAGIC);

	/* Check that the block is properly aligned for the arena. */
	/* 블록이 아레나에 맞게 제대로 정렬되었는지 확인합니다. */
	ASSERT(a->desc == NULL || (pg_ofs(b) - sizeof *a) % a->desc->block_size == 0);
	ASSERT(a->desc != NULL || pg_ofs(b) == sizeof *a);

	return a;
}

/* Returns the (IDX - 1)'th block within arena A. */
/* 아레나 A 내의 (IDX - 1)'번째 블록을 반환합니다. */
static struct block *arena_to_block(struct arena *a, size_t idx)
{
	ASSERT(a != NULL);
	ASSERT(a->magic == ARENA_MAGIC);
	ASSERT(idx < a->desc->blocks_per_arena);
	return (struct block *)((uint8_t *)a + sizeof *a + idx * a->desc->block_size);
}
