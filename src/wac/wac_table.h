#ifndef __WAC_TABLE_H
#define __WAC_TABLE_H

typedef struct wac_table_entry_s wac_table_entry_t;

typedef struct wac_table_s {
	size_t asize, usize;
	wac_table_entry_t *entries;
} wac_table_t;

#include "wac_common.h"
#include "wac_value.h"
#include "wac_object.h"

#define WAC_TABLE_MAX_LOAD 0.75

struct wac_table_entry_s {
	wac_obj_string_t *key;
	wac_value_t value;
};


void wac_table_init(wac_state_t *state, wac_table_t *table);
wac_obj_string_t* wac_table_find_string(wac_table_t *table, const char *buf, size_t len, uint32_t hash);
bool wac_table_set(wac_state_t *state, wac_table_t *table, wac_obj_string_t *key, wac_value_t value);
void wac_table_addAll(wac_state_t *state, wac_table_t *src, wac_table_t *dst);
bool wac_table_get(wac_table_t *table, wac_obj_string_t *key, wac_value_t *value);
bool wac_table_delete(wac_table_t *table, wac_obj_string_t *key);
void wac_table_free(wac_state_t *state, wac_table_t *table);

#endif //__WAC_TABLE_H
