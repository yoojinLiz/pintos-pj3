/* file.c: Implementation of memory backed file object (mmaped object). */

#include <list.h>
#include "vm/vm.h"
#include "threads/vaddr.h"
#include "lib/string.h"
#include "lib/round.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "threads/mmu.h"


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;

	/* Store uninit page elements for page copy. */
	file_page->init = page->uninit.init;
	file_page->type = page->uninit.type;
	file_page->aux = page->uninit.aux;
	file_page->page_initializer = page->uninit.page_initializer;

    return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	struct supplemental_page_table *spt = &thread_current()->spt;

	/* TODO: For swap-in case. (Must not enter in first page fault case) */

	/* Load aux data. */
	struct aux_data *aux = file_page->aux;
	struct file *file = aux->file;
	off_t ofs = aux->ofs;
	size_t page_read_bytes = aux->page_read_bytes;
	size_t page_zero_bytes = aux->page_zero_bytes;
	
	/* Load this page. */
	if (file_read_at (file, page->va, page_read_bytes, ofs) != (int) page_read_bytes)
		return false;

	memset (page->va + page_read_bytes, 0, page_zero_bytes);

	/* Set dirty bit to old one. */
	pml4_set_dirty (thread_current()->pml4, page->va, 0);

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if (page->frame){
	page->frame->page = NULL;		
	}
	free(page->frame);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,struct file *file, off_t offset) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	void *start_addr = addr;
	int page_cnt = 0;

	/* Set read_bytes and zero_bytes. */
	uint32_t read_bytes, zero_bytes, readable_bytes;
	if ((readable_bytes = file_length (file) - offset) <= 0)
		return NULL;
	read_bytes = length <= readable_bytes ? length : readable_bytes;
	zero_bytes = PGSIZE - (read_bytes % PGSIZE);

	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);

	/* Check the addresses to prevent overlap any existing pages. */
	for (; addr < (start_addr + read_bytes + zero_bytes); addr += PGSIZE) {
		/* Also, count pages here. */
		page_cnt += 1;
		if (spt_find_page (spt, addr))
			return NULL;
	}

	/* Allocate each page with initializer. */
	addr = start_addr;
	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Setup auxiliary data. We will use reopened file
		   because file may closed or removed by other process.*/
		struct aux_data *aux = calloc (1, sizeof(struct aux_data));
		aux->file = file;
		aux->ofs = offset;
		aux->page_read_bytes = page_read_bytes;
		aux->page_zero_bytes = page_zero_bytes;

		/* Call the alloc function. */
		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable,file_backed_swap_in, aux))
			PANIC("do_mmap: vm_alloc_page failed.");

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += PGSIZE;
	}

	/* Push the start page into mmap list. */
	struct page *start_page = spt_find_page (spt, start_addr);
	start_page->page_cnt = page_cnt;
	list_push_back (&spt->mmap_list, &start_page->mmap_elem);

	return start_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	uint64_t *pml4 = thread_current()->pml4;
	struct page *page;
	int page_cnt;
	struct aux_data *aux;
	struct file *file;
	void *start_addr = addr;

	page = spt_find_page (spt, start_addr);
	page_cnt = page->page_cnt;
	
	/* Iterate as page counts. */
	for (int i=0; i < page_cnt; i++) {
		page = spt_find_page (spt, addr);
		aux = page->file.aux;
		file = aux->file;

		/* Stuff below is only for the initialized pages. */
		if (page->frame) {
			/* If the page is dirty, write back to the file. */
			if (pml4_is_dirty (pml4, addr)){
				lock_acquire(&file_lock);
				file_write_at (file, addr, aux->page_read_bytes, aux->ofs);
				lock_release(&file_lock);
			}
			/* Remove page from pml4. */
			pml4_clear_page (pml4, addr);
		}

		/* Advance. */
		addr += PGSIZE;
	}
	}
