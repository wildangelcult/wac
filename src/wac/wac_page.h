#ifndef __WAC_PAGE_H
#define __WAC_PAGE_H

#include "wac_common.h"
#include "wac_value.h"

typedef struct wac_vm_s wac_vm_t;

typedef enum wac_opCode_e {
	WAC_OP_CONST,
	WAC_OP_NULL,
	WAC_OP_TRUE,
	WAC_OP_FALSE,
	WAC_OP_CLOSURE,

	WAC_OP_POP,
	WAC_OP_POPN,

	WAC_OP_GET_LOCAL,
	WAC_OP_SET_LOCAL,
	WAC_OP_GET_UPVAL,
	WAC_OP_SET_UPVAL,
	WAC_OP_GET_GLOBAL,
	WAC_OP_SET_GLOBAL,
	WAC_OP_CLOSE_UPVAL,
	WAC_OP_DEFINE_GLOBAL,

	WAC_OP_NOT,
	WAC_OP_EQUAL,
	WAC_OP_GREATER,
	WAC_OP_LESS,
	WAC_OP_NEG,
	WAC_OP_ADD,
	WAC_OP_SUB,
	WAC_OP_MUL,
	WAC_OP_DIV,

	WAC_OP_JMP_FORW,
	WAC_OP_JMP_BACK,
	WAC_OP_JMP_TRUE,
	WAC_OP_JMP_FALSE,

	WAC_OP_CALL,

	WAC_OP_RET,
} wac_opCode_t;

typedef struct wac_page_s {
	size_t asize, usize;
	uint8_t *code;
	size_t *lines;
	wac_valarr_t consts;
} wac_page_t;

void wac_page_init(wac_state_t *state, wac_page_t *page);
void wac_page_write_byte(wac_state_t *state, wac_page_t *page, uint8_t byte, size_t line);
uint32_t wac_page_addConst(wac_state_t *state, wac_page_t *page, wac_value_t value);
void wac_page_write_4bytes(wac_state_t *state, wac_page_t *page, uint32_t constant, size_t line);
void wac_page_free(wac_state_t *state, wac_page_t *page);

#endif //__WAC_PAGE_H
