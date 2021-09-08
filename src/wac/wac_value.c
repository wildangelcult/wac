#include <stdio.h>
#include <string.h>

#include "wac_state.h"
#include "wac_value.h"
#include "wac_object.h"
#include "wac_memory.h"

void wac_value_print(wac_value_t value) {
	switch (value.type) {
		case WAC_VAL_TYPE_NULL: printf("null"); break;
		case WAC_VAL_TYPE_BOOL: printf(WAC_VAL_AS_BOOL(value) ? "true" : "false"); break;
		case WAC_VAL_TYPE_NUMBER: printf("%g", WAC_VAL_AS_NUMBER(value)); break;
		case WAC_VAL_TYPE_OBJ: wac_obj_print(value); break;
	}
}

bool wac_value_falsey(wac_value_t value) {
	return WAC_VAL_IS_NULL(value) || (WAC_VAL_IS_BOOL(value) && !WAC_VAL_AS_BOOL(value));
}

bool wac_value_equal(wac_value_t a, wac_value_t b) {
	if (a.type != b.type) return false;

	switch (a.type) {
		case WAC_VAL_TYPE_NULL: return true;
		case WAC_VAL_TYPE_BOOL: return WAC_VAL_AS_BOOL(a) == WAC_VAL_AS_BOOL(b);
		case WAC_VAL_TYPE_NUMBER: return WAC_VAL_AS_NUMBER(a) == WAC_VAL_AS_NUMBER(b);
		case WAC_VAL_TYPE_OBJ: return WAC_VAL_AS_OBJ(a) == WAC_VAL_AS_OBJ(b);
		default: return false;
	}
}

void wac_valarr_init(wac_state_t *state, wac_valarr_t *valarr) {
	valarr->asize = WAC_ARRAY_DEFAULT_SIZE;
	valarr->usize = 0;
	valarr->values = WAC_ARRAY_INIT(state, wac_value_t, valarr->asize);
}

void wac_valarr_write(wac_state_t *state, wac_valarr_t *valarr, wac_value_t value) {
	if (valarr->asize <= valarr->usize) {
		size_t oldSize = valarr->asize;
		valarr->asize *= WAC_ARRAY_GROW_MUL;
		valarr->values = WAC_ARRAY_GROW(state, wac_value_t, valarr->values, oldSize, valarr->asize);
	}

	valarr->values[valarr->usize++] = value;
}

void wac_valarr_free(wac_state_t *state, wac_valarr_t *valarr) {
	WAC_ARRAY_FREE(state, wac_value_t, valarr->values, valarr->asize);
	valarr->asize = 0;
	valarr->usize = 0;
	valarr->values = NULL;
}
