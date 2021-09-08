#include <stdio.h>
#include <string.h>

#include "wac_state.h"
#include "wac_object.h"
#include "wac_memory.h"
#include "wac_vm.h"
#include "wac_table.h"

#define WAC_OBJ_ALLOC(type, objType) (type*)wac_obj_alloc(state, sizeof(type), objType)

static wac_obj_t* wac_obj_alloc(wac_state_t *state, size_t size, wac_obj_type_t type) {
	wac_obj_t *obj = (wac_obj_t*)wac_realloc(state, NULL, 0, size);
	obj->type = type;
	obj->isMarked = false;
	obj->next = state->vm.objs;
	state->vm.objs = obj;
#ifdef WAC_DEBUG_GC_LOG
	printf("[*] Allocated %u bytes for object %p of type %d\n", size, obj, type);
#endif
	return obj;
}

static wac_obj_string_t* wac_obj_string_alloc(wac_state_t *state, char *buf, size_t len, uint32_t hash) {
	wac_obj_string_t *string = WAC_OBJ_ALLOC(wac_obj_string_t, WAC_OBJ_STRING);
	string->len = len;
	string->buf = buf;
	string->hash = hash;
	wac_vm_push(&state->vm, WAC_VAL_OBJ(string));
	wac_table_set(state, &state->vm.strings, string, WAC_VAL_NULL);
	wac_vm_pop(&state->vm);
	return string;
}

static uint32_t wac_obj_string_hash(const char *string, size_t len) {
	uint32_t hash = 2166136261u;
	size_t i;
	for (i = 0; i < len; ++i) {
		hash ^= (uint8_t)string[i];
		hash *= 16777619;
	}
	return hash;
}

wac_obj_string_t* wac_obj_string_copy(wac_state_t *state, const char *src, size_t len) {
	char *dst = WAC_ARRAY_INIT(state, char, len + 1);
	uint32_t hash = wac_obj_string_hash(src, len);

	wac_obj_string_t *interned = wac_table_find_string(&state->vm.strings, src, len, hash);
	if (interned) return interned;

	memcpy(dst, src, len);
	dst[len] = '\0';
	return wac_obj_string_alloc(state, dst, len, hash);
}

wac_obj_string_t* wac_obj_string_take(wac_state_t *state, char *buf, size_t len) {
	uint32_t hash = wac_obj_string_hash(buf, len);
	wac_obj_string_t *interned = wac_table_find_string(&state->vm.strings, buf, len, hash);
	if (interned) {
		WAC_ARRAY_FREE(state, char, buf, len + 1);
		return interned;
	}
	return wac_obj_string_alloc(state, buf, len, hash);
}

static void wac_obj_fun_print(wac_obj_fun_t *fun) {
	if (fun->name) {
		printf("<fun %s>", fun->name->buf);
	} else {
		printf("<script>");
	}
}

void wac_obj_print(wac_value_t value) {
	switch (WAC_OBJ_TYPE(value)) {
		case WAC_OBJ_STRING:
			printf("%s", WAC_OBJ_AS_STRING(value)->buf);
			break;
		case WAC_OBJ_FUN:
			wac_obj_fun_print(WAC_OBJ_AS_FUN(value));
			break;
		case WAC_OBJ_NATIVE:
			printf("<native fun %s>", WAC_OBJ_AS_NATIVE(value)->name->buf);
			break;
		case WAC_OBJ_CLOSURE:
			wac_obj_fun_print(WAC_OBJ_AS_CLOSURE(value)->fun);
			break;
		case WAC_OBJ_UPVAL:
			printf("<upval>");
			break;
	}
}

wac_obj_fun_t* wac_obj_fun_init(wac_state_t *state) {
	wac_obj_fun_t *fun = WAC_OBJ_ALLOC(wac_obj_fun_t, WAC_OBJ_FUN);
	fun->arity = 0;
	fun->upvals_usize = 0;
	fun->name = NULL;
	//coz the page_init might trigger wac_realloc
	wac_vm_push(&state->vm, WAC_VAL_OBJ(fun));
	wac_page_init(state, &fun->page);
	wac_vm_pop(&state->vm);
	return fun;
}

wac_obj_native_t* wac_obj_native_init(wac_state_t *state, uint32_t arity, wac_obj_string_t *name, wac_native_fun_t fun) {
	wac_obj_native_t* native = WAC_OBJ_ALLOC(wac_obj_native_t, WAC_OBJ_NATIVE);
	native->arity = arity;
	native->name = name;
	native->fun = fun;
	return native;
}

wac_obj_closure_t* wac_obj_closure_init(wac_state_t *state, wac_obj_fun_t *fun) {
	size_t i;
	wac_obj_upval_t **upvals = WAC_ARRAY_INIT(state, wac_obj_upval_t*, fun->upvals_usize);
	for (i = 0; i < fun->upvals_usize; ++i) upvals[i] = NULL;

	wac_obj_closure_t *closure = WAC_OBJ_ALLOC(wac_obj_closure_t, WAC_OBJ_CLOSURE);
	closure->fun = fun;
	closure->upvals_usize = fun->upvals_usize;
	closure->upvals = upvals;
	return closure;
}

wac_obj_upval_t* wac_obj_upval_init(wac_state_t *state, wac_value_t *loc) {
	wac_obj_upval_t *upval = WAC_OBJ_ALLOC(wac_obj_upval_t, WAC_OBJ_UPVAL);
	upval->closed = WAC_VAL_NULL;
	upval->loc = loc;
	upval->next = NULL;
	return upval;
}

void wac_obj_free(wac_state_t *state, wac_obj_t *obj) {
#ifdef WAC_DEBUG_GC_LOG
	printf("[*] Freed object %p of type %d\n", obj, obj->type);
#endif
	switch (obj->type) {
		case WAC_OBJ_STRING: {
			wac_obj_string_t *string = (wac_obj_string_t*)obj;
			WAC_ARRAY_FREE(state, char, string->buf, string->len + 1);
			WAC_FREE(state, wac_obj_string_t, obj);
			break;
		}
		case WAC_OBJ_FUN: {
			wac_obj_fun_t *fun = (wac_obj_fun_t*)obj;
			wac_page_free(state, &fun->page);
			WAC_FREE(state, wac_obj_fun_t, obj);
			break;
		}
		case WAC_OBJ_NATIVE:
			WAC_FREE(state, wac_obj_native_t, obj);
			break;
		case WAC_OBJ_CLOSURE: {
			wac_obj_closure_t *closure = (wac_obj_closure_t*)obj;
			WAC_ARRAY_FREE(state, wac_obj_upval_t*, closure->upvals, closure->upvals_usize);
			WAC_FREE(state, wac_obj_closure_t, obj);
			break;
		}
		case WAC_OBJ_UPVAL:
			WAC_FREE(state, wac_obj_upval_t, obj);
			break;
	}
}
