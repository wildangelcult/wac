#include <stdlib.h>

#include "wac_state.h"
#include "wac_page.h"
#include "wac_memory.h"

void wac_page_init(wac_state_t *state, wac_page_t *page) {
	page->asize = WAC_ARRAY_DEFAULT_SIZE;
	page->usize = 0;
	//coz gc
	page->consts.usize = 0;
	page->code = WAC_ARRAY_INIT(state, uint8_t, page->asize);
	page->lines = WAC_ARRAY_INIT(state, size_t, page->asize);
	wac_valarr_init(state, &page->consts);
}

void wac_page_write_byte(wac_state_t *state, wac_page_t *page, uint8_t byte, size_t line) {
	if (page->asize <= page->usize) {
		size_t oldSize = page->asize;
		page->asize *= WAC_ARRAY_GROW_MUL;
		page->code = WAC_ARRAY_GROW(state, uint8_t, page->code, oldSize, page->asize);
		page->lines = WAC_ARRAY_GROW(state, size_t, page->lines, oldSize, page->asize);
	}

	page->code[page->usize] = byte;
	page->lines[page->usize] = line;
	page->usize++;
}

uint32_t wac_page_addConst(wac_state_t *state, wac_page_t *page, wac_value_t value) {
	wac_vm_push(&state->vm, value);
	wac_valarr_write(state, &page->consts, value);
	wac_vm_pop(&state->vm);
	return page->consts.usize - 1;
}

void wac_page_write_4bytes(wac_state_t *state, wac_page_t *page, uint32_t bytes, size_t line) {
	wac_page_write_byte(state, page, (bytes & 0xFF000000) >> 24, line);
	wac_page_write_byte(state, page, (bytes & 0x00FF0000) >> 16, line);
	wac_page_write_byte(state, page, (bytes & 0x0000FF00) >> 8, line);
	wac_page_write_byte(state, page, (bytes & 0x000000FF), line);
}

void wac_page_free(wac_state_t *state, wac_page_t *page) {
	WAC_ARRAY_FREE(state, uint8_t, page->code, page->asize);
	WAC_ARRAY_FREE(state, size_t, page->lines, page->asize);
	page->asize = 0;
	page->usize = 0;
	page->code = NULL;
	wac_valarr_free(state, &page->consts);
}
