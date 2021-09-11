#ifndef __WAC_OBJECT_H
#define __WAC_OBJECT_H

#include "wac_common.h"
#include "wac_state.h"
#include "wac_page.h"
#include "wac_value.h"

typedef enum wac_obj_type_e {
	WAC_OBJ_STRING,
	WAC_OBJ_FUN,
	WAC_OBJ_NATIVE,
	WAC_OBJ_CLOSURE,
	WAC_OBJ_UPVAL,
} wac_obj_type_t;

typedef struct wac_obj_s {
	wac_obj_type_t type;
	bool isMarked;
	struct wac_obj_s *next;
} wac_obj_t;

typedef struct wac_obj_string_s {
	wac_obj_t obj;
	size_t len;
	char *buf;
	uint32_t hash;
} wac_obj_string_t;

typedef struct wac_obj_fun_s {
	wac_obj_t obj;
	uint32_t arity;
	size_t upvals_usize;
	wac_page_t page;
	wac_obj_string_t *name;
} wac_obj_fun_t;

typedef wac_value_t (*wac_native_fun_t)(uint32_t argc, wac_value_t *argv);

typedef struct wac_obj_native_s {
	wac_obj_t obj;
	uint32_t arity;
	wac_obj_string_t *name;
	wac_native_fun_t fun;
} wac_obj_native_t;

typedef struct wac_obj_upval_s {
	wac_obj_t obj;
	wac_value_t closed;
	wac_value_t *loc;
	struct wac_obj_upval_s *next;
} wac_obj_upval_t;

typedef struct wac_obj_closure_s {
	wac_obj_t obj;
	wac_obj_fun_t *fun;
	size_t upvals_usize;
	wac_obj_upval_t **upvals;
} wac_obj_closure_t;

#define WAC_OBJ_TYPE(value) (WAC_VAL_AS_OBJ(value)->type)

#define WAC_OBJ_IS_STRING(value) wac_obj_isType(value, WAC_OBJ_STRING)
#define WAC_OBJ_IS_FUN(value) wac_obj_isType(value, WAC_OBJ_FUN)
#define WAC_OBJ_IS_NATIVE(value) wac_obj_isType(value, WAC_OBJ_NATIVE)
#define WAC_OBJ_IS_CLOSURE(value) wac_obj_isType(value, WAC_OBJ_CLOSURE;)

#define WAC_OBJ_AS_STRING(value) ((wac_obj_string_t*)WAC_VAL_AS_OBJ(value))
#define WAC_OBJ_AS_FUN(value) ((wac_obj_fun_t*)WAC_VAL_AS_OBJ(value))
#define WAC_OBJ_AS_NATIVE(value) ((wac_obj_native_t*)WAC_VAL_AS_OBJ(value))
#define WAC_OBJ_AS_CLOSURE(value) ((wac_obj_closure_t*)WAC_VAL_AS_OBJ(value))

static inline bool wac_obj_isType(wac_value_t value, wac_obj_type_t type) {
	return WAC_VAL_IS_OBJ(value) && WAC_OBJ_TYPE(value) == type;
}
wac_obj_string_t* wac_obj_string_copy(wac_state_t *state, const char *src, size_t len);
void wac_obj_print(wac_value_t value);
wac_obj_string_t* wac_obj_string_take(wac_state_t *state, char *buf, size_t len);
wac_obj_fun_t* wac_obj_fun_init(wac_state_t *state);
wac_obj_native_t* wac_obj_native_init(wac_state_t *state, uint32_t arity, wac_obj_string_t *name, wac_native_fun_t fun);
wac_obj_closure_t* wac_obj_closure_init(wac_state_t *state, wac_obj_fun_t *fun);
wac_obj_upval_t* wac_obj_upval_init(wac_state_t *state, wac_value_t *loc);
void wac_obj_free(wac_state_t *state, wac_obj_t *obj);

#endif //__WAC_OBJECT_H
