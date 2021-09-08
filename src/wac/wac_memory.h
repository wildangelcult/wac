#ifndef __WAC_MEMORY_H
#define __WAC_MEMORY_H

#include "wac_common.h"
#include "wac_state.h"

#define WAC_ARRAY_DEFAULT_SIZE	8
#define WAC_ARRAY_GROW_MUL	2
#define WAC_GC_GROW_MUL		2

#define WAC_ARRAY_INIT(state, type, newSize) (type*)wac_realloc(state, NULL, 0, sizeof(type) * (newSize))
#define WAC_ARRAY_GROW(state, type, ptr, oldSize, newSize) (type*)wac_realloc(state, ptr, sizeof(type) * (oldSize), sizeof(type) * (newSize))
#define WAC_ARRAY_FREE(state, type, ptr, oldSize) (type*)wac_realloc(state, ptr, sizeof(type) * (oldSize), 0)
#define WAC_FREE(state, type, ptr) (type*)wac_realloc(state, ptr, sizeof(type), 0)

#define WAC_ARRAY_INIT_NOGC(type, size) (type*)malloc(sizeof(type) * (size))
#define WAC_ARRAY_GROW_NOGC(type, ptr, size) (type*)realloc(ptr, sizeof(type) * (size))

void* wac_realloc(wac_state_t *state, void *ptr, size_t oldSize, size_t newSize);

#endif //__WAC_MEMORY_H
