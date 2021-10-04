#ifndef __WAC_VM_H
#define __WAC_VM_H

#include "wac_common.h"
#include "wac_page.h"
#include "wac_value.h"
#include "wac_table.h"
#include "wac_object.h"

//#define WAC_STACK_MAX 256

typedef struct wac_frame_s {
	wac_obj_closure_t *closure;
	uint8_t *ip;
	wac_value_t *bp;
} wac_frame_t;

struct wac_vm_s {
	size_t frames_asize, frames_usize;
	wac_frame_t *frames;

	size_t stack_asize;
	wac_value_t *stack;
	wac_value_t *sp;

	wac_table_t globals;
	wac_table_t strings;
	wac_obj_string_t *initString;
	wac_obj_upval_t *openUpvals;
	wac_obj_t *objs;

	size_t mem_total, mem_nextGC;

	size_t grays_asize, grays_usize;
	wac_obj_t **grays;
};


typedef enum wac_interpretResult_e {
	WAC_INTERPRET_OK,
	WAC_INTERPRET_COMPILE_ERROR,
	WAC_INTERPRET_RUNTIME_ERROR
} wac_interpretResult_t;

void wac_vm_init(wac_state_t *state);
void wac_defineNativeFun(wac_state_t *state, uint32_t arity, const char *name, wac_native_fun_t fun);
wac_interpretResult_t wac_interpret(wac_state_t *state, const char *src);
void wac_vm_push(wac_vm_t *vm, wac_value_t value);
wac_value_t wac_vm_pop(wac_vm_t *vm);
void wac_vm_free(wac_state_t *state);

#endif //__WAC_VM_H
