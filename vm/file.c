/* file.c: Implementation of memory backed file object (mmaped object). */

#include <round.h>
#include "vm/vm.h"
#include "threads/mmu.h"
#include "userprog/process.h"

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
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
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
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	size_t read_bytes = length;
  	size_t zero_bytes = ROUND_UP (length, PGSIZE) - length;

	void *cur = addr;
	struct supplemental_page_table *spt = &thread_current ()->spt.hash_spt;
	for (size_t cur_ = 0; cur_ < length; cur_ += PGSIZE)
		if (spt_find_page (spt, addr + cur_))
			return NULL; // 하나라도 겹치면 안되므로! 

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct aux_data *aux = (struct aux_data *)calloc(1, sizeof(struct aux_data));
		if (aux == NULL)
			return NULL;

		aux->file = file;
		aux->page_read_bytes = page_read_bytes;
		aux->page_zero_bytes = page_zero_bytes;
		aux->ofs = offset;	

		if (!vm_alloc_page_with_initializer (VM_FILE, cur, writable, lazy_load_segment, aux)) {
			return NULL;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		offset += PGSIZE;
		addr += PGSIZE;
		cur += PGSIZE;
		}

	/* 성공하면 파일이 매핑된 가상 주소를 반환해야 해 ... */
	return addr ; 
	}

/* Do the munmap */
void
do_munmap (void *addr) {
}
