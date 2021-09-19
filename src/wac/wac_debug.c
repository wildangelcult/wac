#include <stdio.h>

#include "wac_state.h"
#include "wac_debug.h"
#include "wac_object.h"

static size_t wac_inst_simple(const char *name, size_t address);
static size_t wac_inst_const(const char *name, size_t address, wac_page_t *page);
static size_t wac_inst_bytes(const char *name, size_t address, wac_page_t *page);
static size_t wac_inst_jmp_forw(const char *name, size_t address, wac_page_t *page);
static size_t wac_inst_jmp_back(const char *name, size_t address, wac_page_t *page);

void wac_page_disass(wac_page_t *page, const char *name) {
	printf("== %s ==\n", name);
	size_t address;
	for (address = 0; address < page->usize; ) {
		address = wac_inst_disass(page, address);
	}
}


size_t wac_inst_disass(wac_page_t *page, size_t address) {
	printf("%08x ", address);

	if (address != 0 && page->lines[address] == page->lines[address - 1]) {
		printf("   | ");
	} else {
		printf("%4u ", page->lines[address]);
	}

	switch (page->code[address]) {
		case WAC_OP_CONST:
			return wac_inst_const("WAC_OP_CONST", address, page);
		case WAC_OP_NULL:
			return wac_inst_simple("WAC_OP_NULL", address);
		case WAC_OP_TRUE:
			return wac_inst_simple("WAC_OP_TRUE", address);
		case WAC_OP_FALSE:
			return wac_inst_simple("WAC_OP_FALSE", address);
		case WAC_OP_CLOSURE: {
#define WAC_READ_4_BYTES() ((uint32_t)((page->code[address++] << 24) | (page->code[address++] << 16) | (page->code[address++] << 8) | page->code[address++]))
			++address;
			uint32_t constant = WAC_READ_4_BYTES();
			printf("%-20s %u ", "WAC_OP_CLOSURE", constant);
			wac_value_print(page->consts.values[constant]);
			printf("\n");

			size_t i;
			uint8_t isLocal;
			uint32_t index;
			wac_obj_fun_t *fun = WAC_OBJ_AS_FUN(page->consts.values[constant]);
			
			for (i = 0; i < fun->upvals_usize; ++i) {
				isLocal = page->code[address++];
				index = WAC_READ_4_BYTES();
				printf("%08x      |                         %s %u\n", address - 5, isLocal ? "local" : "upval", index);
			}

			return address;
#undef WAC_READ_4_BYTES
		}
		case WAC_OP_CLASS:
			return wac_inst_const("WAC_OP_CLASS", address, page);
		case WAC_OP_POP:
			return wac_inst_simple("WAC_OP_POP", address);
		case WAC_OP_POPN:
			return wac_inst_bytes("WAC_OP_POPN", address, page);
		case WAC_OP_GET_LOCAL:
			return wac_inst_bytes("WAC_OP_GET_LOCAL", address, page);
		case WAC_OP_SET_LOCAL:
			return wac_inst_bytes("WAC_OP_SET_LOCAL", address, page);
		case WAC_OP_GET_UPVAL:
			return wac_inst_bytes("WAC_OP_GET_UPVAL", address, page);
		case WAC_OP_SET_UPVAL:
			return wac_inst_bytes("WAC_OP_SET_UPVAL", address, page);
		case WAC_OP_GET_GLOBAL:
			return wac_inst_const("WAC_OP_GET_GLOBAL", address, page);
		case WAC_OP_SET_GLOBAL:
			return wac_inst_const("WAC_OP_SET_GLOBAL", address, page);
		case WAC_OP_GET_PROPERTY:
			return wac_inst_simple("WAC_OP_GET_PROPERTY", address);
		case WAC_OP_SET_PROPERTY:
			return wac_inst_simple("WAC_OP_SET_PROPERTY", address);
		case WAC_OP_CLOSE_UPVAL:
			return wac_inst_simple("WAC_OP_CLOSE_UPVAL", address);
		case WAC_OP_DEFINE_GLOBAL:
			return wac_inst_const("WAC_OP_DEFINE_GLOBAL", address, page);
		case WAC_OP_NOT:
			return wac_inst_simple("WAC_OP_NOT", address);
		case WAC_OP_EQUAL:
			return wac_inst_simple("WAC_OP_EQUAL", address);
		case WAC_OP_GREATER:
			return wac_inst_simple("WAC_OP_GREATER", address);
		case WAC_OP_LESS:
			return wac_inst_simple("WAC_OP_LESS", address);
		case WAC_OP_NEG:
			return wac_inst_simple("WAC_OP_NEG", address);
		case WAC_OP_ADD:
			return wac_inst_simple("WAC_OP_ADD", address);
		case WAC_OP_SUB:
			return wac_inst_simple("WAC_OP_SUB", address);
		case WAC_OP_MUL:
			return wac_inst_simple("WAC_OP_MUL", address);
		case WAC_OP_DIV:
			return wac_inst_simple("WAC_OP_DIV", address);
		case WAC_OP_JMP_FORW:
			return wac_inst_jmp_forw("WAC_OP_JMP_FORW", address, page);
		case WAC_OP_JMP_BACK:
			return wac_inst_jmp_back("WAC_OP_JMP_BACK", address, page);
		case WAC_OP_JMP_TRUE:
			return wac_inst_jmp_forw("WAC_OP_JMP_TRUE", address, page);
		case WAC_OP_JMP_FALSE:
			return wac_inst_jmp_forw("WAC_OP_JMP_FALSE", address, page);
		case WAC_OP_CALL:
			return wac_inst_bytes("WAC_OP_CALL", address, page);
		case WAC_OP_RET:
			return wac_inst_simple("WAC_OP_RET", address);
		default:
			fprintf(stderr, "[-] Unknown instruction %u\n", page->code[address]);
			return address + 1;
	}
}

#define WAC_READ_4_BYTES() ((uint32_t)((page->code[address + 1] << 24) | (page->code[address + 2] << 16) | (page->code[address + 3] << 8) | page->code[address + 4]))

static size_t wac_inst_simple(const char *name, size_t address) {
	printf("%s\n", name);
	return address + 1;
}

static size_t wac_inst_const(const char *name, size_t address, wac_page_t *page) {
	uint32_t constant = WAC_READ_4_BYTES();
	printf("%-20s %u '", name, constant);
	wac_value_print(page->consts.values[constant]);
	printf("'\n");
	return address + 5;
}

static size_t wac_inst_bytes(const char *name, size_t address, wac_page_t *page) {
	printf("%-20s %u\n", name, WAC_READ_4_BYTES());
	return address + 5;
}

static size_t wac_inst_jmp_forw(const char *name, size_t address, wac_page_t *page) {
	printf("%-20s %08x -> %08x\n", name, address, address + 5 + WAC_READ_4_BYTES());
	return address + 5;
}

static size_t wac_inst_jmp_back(const char *name, size_t address, wac_page_t *page) {
	printf("%-20s %08x -> %08x\n", name, address, address + 5 - WAC_READ_4_BYTES());
	return address + 5;
}
