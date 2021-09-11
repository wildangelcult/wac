#ifndef __WAC_VALUE_H
#define __WAC_VALUE_H

#include "wac_common.h"
#include "wac_object.h"

typedef enum wac_value_type_e {
	WAC_VAL_TYPE_NULL,
	WAC_VAL_TYPE_BOOL,
	WAC_VAL_TYPE_NUMBER,
	WAC_VAL_TYPE_OBJ,
} wac_value_type_t;

typedef struct wac_value_s {
	wac_value_type_t type;
	union {
		bool b;
		double n;
		wac_obj_t *o;
	} as;
} wac_value_t;

#define WAC_VAL_NULL ((wac_value_t){WAC_VAL_TYPE_NULL, {.n = 0}})
#define WAC_VAL_BOOL(value) ((wac_value_t){WAC_VAL_TYPE_BOOL, {.b = (value)}})
#define WAC_VAL_NUMBER(value) ((wac_value_t){WAC_VAL_TYPE_NUMBER, {.n = (value)}})
#define WAC_VAL_OBJ(value) ((wac_value_t){WAC_VAL_TYPE_OBJ, {.o = (wac_obj_t*)(value)}})

#define WAC_VAL_AS_BOOL(value) ((value).as.b)
#define WAC_VAL_AS_NUMBER(value) ((value).as.n)
#define WAC_VAL_AS_OBJ(value) ((value).as.o)

#define WAC_VAL_IS_NULL(value) ((value).type == WAC_VAL_TYPE_NULL)
#define WAC_VAL_IS_BOOL(value) ((value).type == WAC_VAL_TYPE_BOOL)
#define WAC_VAL_IS_NUMBER(value) ((value).type == WAC_VAL_TYPE_NUMBER)
#define WAC_VAL_IS_OBJ(value) ((value).type == WAC_VAL_TYPE_OBJ)

typedef struct wac_valarr_s {
	size_t asize, usize;
	wac_value_t *values;
} wac_valarr_t;

void wac_value_print(wac_value_t value);
bool wac_value_falsey(wac_value_t value);
bool wac_value_equal(wac_value_t a, wac_value_t b);
void wac_valarr_init(wac_state_t *state, wac_valarr_t *valarr);
void wac_valarr_write(wac_state_t *state, wac_valarr_t *valarr, wac_value_t value);
void wac_valarr_free(wac_state_t *state, wac_valarr_t *valarr);

#endif //__WAC_VALUE_H
