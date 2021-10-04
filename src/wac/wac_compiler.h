#ifndef __WAC_COMPILER_H
#define __WAC_COMPILER_H

#include "wac_common.h"
#include "wac_scanner.h"
#include "wac_object.h"

typedef struct wac_parser_s {
	wac_token_t curr, prev;
	bool error, panic;
} wac_parser_t;

typedef struct wac_local_s {
	wac_token_t name;
	unsigned int depth;
	bool isCaptured;
} wac_local_t;

typedef enum wac_fun_type_e {
	WAC_FUN_TYPE_SCRIPT,
	WAC_FUN_TYPE_FUN,
	WAC_FUN_TYPE_METHOD,
	WAC_FUN_TYPE_INIT,
} wac_fun_type_t;

typedef struct wac_upval_s {
	uint32_t index;
	bool isLocal;
} wac_upval_t;

typedef struct wac_compiler_s {
	struct wac_compiler_s* prev;

	wac_obj_fun_t *fun;
	wac_fun_type_t type;

	size_t locals_asize, locals_usize;
	wac_local_t *locals;

	size_t upvals_asize;
	wac_upval_t *upvals;

	unsigned int scopeDepth;
} wac_compiler_t;

typedef struct wac_class_compiler_s {
	struct wac_class_compiler_s *prev;
} wac_class_compiler_t;

wac_obj_fun_t* wac_compiler_compile(wac_state_t *state, const char *src);

#endif //__WAC_COMPILER_H
