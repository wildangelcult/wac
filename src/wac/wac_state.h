#ifndef __WAC_STATE_H
#define __WAC_STATE_H

typedef struct wac_state_s wac_state_t;

#include "wac_page.h"
#include "wac_vm.h"
#include "wac_scanner.h"
#include "wac_compiler.h"

struct wac_state_s {
	//wac_page_t page;
	wac_vm_t vm;
	wac_scanner_t scanner;
	wac_parser_t parser;
	wac_compiler_t *compiler;
	wac_class_compiler_t *classCompiler;
};

wac_state_t* wac_state_init();
void wac_state_free(wac_state_t *state);

#endif //__WAC_STATE_H
