/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	
	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {

		/* TODO: Create the page, fetch the initialier according to the VM type,
		         and then create "uninit" page struct by calling uninit_new. You
		         should modify the field after calling the uninit_new. */

		struct page * page = (struct page*)malloc(sizeof(struct page)) ;
		bool (*initializer)(struct page *, enum vm_type, void *) ; 

		if (VM_TYPE(type) == VM_ANON) {
			initializer = anon_initializer;
		}
		else if (VM_TYPE(type) == VM_FILE) {
			initializer = file_backed_initializer;
		}
		else {
			goto err ;
		}
		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable; 

		/* TODO: Insert the page into the spt. */
		bool res = spt_insert_page(spt, page);
		struct page *result = spt_find_page(spt, upage);
		if (result == NULL){
			goto err ;
		}
		return true ; 
	}
err:
	return false;
}


/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	struct page p;
	struct hash_elem *e;

	p.va = pg_round_down(va);
	e = hash_find (&spt->hash_spt, &p.hash_elem); 
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	struct hash_elem *result = hash_insert (&spt->hash_spt, &page->hash_elem); // null이면 성공인 것 
	return result == NULL ? true : false ; 
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	struct hash_elem * result =	hash_delete (&spt->hash_spt, &page->hash_elem);
	if (result == NULL) {
		return false; 
	}
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.
 * 물리주소(=커널 가상주소)를 가진 frame 구조체를 malloc으로 할당하는 함수 (매핑 X)*/
static struct frame *
vm_get_frame (void) {
	/* TODO: Fill this function. */
	uint64_t *kva = palloc_get_page(PAL_USER);
	
	if (kva == NULL) {
		 PANIC ("no memory. evict & swap out 필요 ");
	}
	// printf("git rollback~~");
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	frame->kva = kva ;
	frame->page = NULL ; 

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
			
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, addr);
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (page == NULL) {
		return false ; 
	}

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. 
 * 가상주소에 존재하는 페이지를 spt에서 찾은 후 물리메모리 할당 후 매핑 (do_claim)*/
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = spt_find_page (&thread_current () ->spt, va);
	if (page == NULL) {
		 return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. 
 * 페이지에 물리 메모리를 할당(get_frame) 후 매핑 */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame (); // 프레임 할당 
	
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	bool result = pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable); 
	if (result == false) {
		return false ; 
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init (&spt->hash_spt, page_hash, page_less, NULL); 
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
