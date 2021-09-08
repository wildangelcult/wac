#include <string.h>

#include "wac_state.h"
#include "wac_table.h"
#include "wac_memory.h"

void wac_table_init(wac_state_t *state, wac_table_t *table) {
	table->asize = 0;
	table->usize = 0;
	table->entries = WAC_ARRAY_INIT(state, wac_table_entry_t, WAC_ARRAY_DEFAULT_SIZE);
	//this is, coz we are marking table before init
	//and coz for looping through table is usually done using asize
	//this is way other dynamic sized structures are safe
	//(coz they use usize to loop and set usize to 0 before alloc)
	table->asize = WAC_ARRAY_DEFAULT_SIZE;

	size_t i;
	for (i = 0; i < table->asize; ++i) {
		table->entries[i].key = NULL;
		table->entries[i].value = WAC_VAL_NULL;
	}
}

static wac_table_entry_t* wac_table_find(wac_table_entry_t *entries, size_t asize, wac_obj_string_t *key) {
	uint32_t index = key->hash % asize;
	wac_table_entry_t *entry, *tombstone = NULL;

	for (;;) {
		entry = &entries[index];

		if (!entry->key) {
			if (WAC_VAL_IS_NULL(entry->value)) {
				return tombstone ? tombstone : entry;
			} else if (!tombstone) {
				tombstone = entry;
			}
		} else if (entry->key == key) {
			return entry;
		}

		index = (index + 1) % asize;
	}
}

wac_obj_string_t* wac_table_find_string(wac_table_t *table, const char *buf, size_t len, uint32_t hash) {
	if (!table->usize) return NULL;

	uint32_t index = hash % table->asize;
	wac_table_entry_t *entry;

	for (;;) {
		entry = &table->entries[index];

		if (!entry->key || WAC_VAL_IS_NULL(entry->value)) {
			return NULL;
		} else if (entry->key->len == len && entry->key->hash == hash && !memcmp(entry->key->buf, buf, len)) {
			return entry->key;
		}

		index = (index + 1) % table->asize;
	}
}

static void wac_table_adjust(wac_state_t *state, wac_table_t *table) {
	size_t i, asize = table->asize * WAC_ARRAY_GROW_MUL;
	wac_table_entry_t *curr, *dest, *entries = WAC_ARRAY_INIT(state, wac_table_entry_t, asize);
	for (i = 0; i < asize; ++i) {
		entries[i].key = NULL;
		entries[i].value = WAC_VAL_NULL;
	}

	table->usize = 0;
	for (i = 0; i < table->asize; ++i) {
		curr = &table->entries[i];
		if (!curr->key) continue;

		dest = wac_table_find(entries, asize, curr->key);
		dest->key = curr->key;
		dest->value = curr->value;

		++table->usize;
	}

	WAC_ARRAY_FREE(state, wac_table_entry_t, table->entries, table->asize);
	table->entries = entries;
	table->asize = asize;
}

bool wac_table_set(wac_state_t *state, wac_table_t *table, wac_obj_string_t *key, wac_value_t value) {
	if ((table->asize * WAC_TABLE_MAX_LOAD) <= table->usize) {
		wac_table_adjust(state, table);
	}

	wac_table_entry_t *entry = wac_table_find(table->entries, table->asize, key);
	bool newKey = !entry->key;
	if (newKey && WAC_VAL_IS_NULL(entry->value)) ++table->usize;
	entry->key = key;
	entry->value = value;
	return newKey;
}

void wac_table_addAll(wac_state_t *state, wac_table_t *src, wac_table_t *dst) {
	size_t i;
	wac_table_entry_t *entry;
	for (i = 0; i < src->asize; ++i) {
		entry = &src->entries[i];
		if (entry->key) {
			wac_table_set(state, dst, entry->key, entry->value);
		}
	}
}

bool wac_table_get(wac_table_t *table, wac_obj_string_t *key, wac_value_t *value) {
	if (!table->usize) return false;

	wac_table_entry_t *entry = wac_table_find(table->entries, table->asize, key);
	if (!entry->key) return false;

	*value = entry->value;
	return true;
}

bool wac_table_delete(wac_table_t *table, wac_obj_string_t *key) {
	if (!table->usize) return false;

	wac_table_entry_t *entry = wac_table_find(table->entries, table->asize, key);
	if (!entry->key) return false;

	//tombstone
	entry->key = NULL;
	entry->value = WAC_VAL_BOOL(true);
	return true;
}

void wac_table_free(wac_state_t *state, wac_table_t *table) {
	WAC_ARRAY_FREE(state, wac_table_entry_t, table->entries, table->asize);
	table->asize = 0;
	table->usize = 0;
	table->entries = NULL;
}
