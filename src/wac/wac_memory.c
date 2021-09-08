#include <stdio.h>
#include <stdlib.h>

#include "wac_memory.h"

#ifdef WAC_DEBUG_GC_LOG
#include "wac_debug.h"
#endif

static void wac_gc_mark_obj(wac_vm_t *vm, wac_obj_t *obj) {
	if (!obj || obj->isMarked) return;
	obj->isMarked = true;
#ifdef WAC_DEBUG_GC_LOG
	printf("[*] Marked object %p ", obj);
	wac_value_print(WAC_VAL_OBJ(obj));
	printf("\n");
#endif
	
	if (!(obj->type == WAC_OBJ_STRING || obj->type == WAC_OBJ_NATIVE)) {
		if (vm->grays_asize <= vm->grays_usize) {
			vm->grays_asize *= WAC_ARRAY_GROW_MUL;
			vm->grays = WAC_ARRAY_GROW_NOGC(wac_obj_t*, vm->grays, vm->grays_asize);
			if (!vm->grays) {
				fprintf(stderr, "[-] Failed to allocate memory for vm->grays\n");
				exit(1);
			}
		}
		vm->grays[vm->grays_usize++] = obj;
	}
}

static void wac_gc_mark_value(wac_vm_t *vm, wac_value_t value) {
	if (WAC_VAL_IS_OBJ(value)) wac_gc_mark_obj(vm, WAC_VAL_AS_OBJ(value));
}

static void wac_gc_mark_table(wac_vm_t *vm, wac_table_t *table) {
	wac_table_entry_t *entry;
	size_t i;
	for (i = 0; i < table->asize; ++i) {
		entry = &table->entries[i];
		wac_gc_mark_obj(vm, (wac_obj_t*)entry->key);
		wac_gc_mark_value(vm, entry->value);
	}
}

static void wac_gc_mark_roots(wac_state_t *state) {
	size_t i;
	wac_value_t *value;
	wac_obj_upval_t *upval;
	wac_compiler_t *compiler;

	for (value = state->vm.stack; value < state->vm.sp; ++value) {
		wac_gc_mark_value(&state->vm, *value);
	}

	for (i = 0; i < state->vm.frames_usize; ++i) {
		wac_gc_mark_obj(&state->vm, (wac_obj_t*)state->vm.frames[i].closure);
	}

	for (upval = state->vm.openUpvals; upval; upval = upval->next) {
		wac_gc_mark_obj(&state->vm, (wac_obj_t*)upval);
	}

	wac_gc_mark_table(&state->vm, &state->vm.globals);

	for (compiler = state->compiler; compiler; compiler = compiler->prev) {
		wac_gc_mark_obj(&state->vm, (wac_obj_t*)compiler->fun);
	}
}

static void wac_gc_mark_valarr(wac_vm_t *vm, wac_valarr_t *arr) {
	size_t i;
	for (i = 0; i < arr->usize; ++i) {
		wac_gc_mark_value(vm, arr->values[i]);
	}
}

static void wac_gc_blacken(wac_vm_t *vm, wac_obj_t *obj) {
#ifdef WAC_DEBUG_GC_LOG
	printf("[*] Blackened object %p ", obj);
	wac_value_print(WAC_VAL_OBJ(obj));
	printf("\n");
#endif
	switch (obj->type) {
		case WAC_OBJ_STRING:
		case WAC_OBJ_NATIVE:
			break;
		case WAC_OBJ_FUN: {
			wac_obj_fun_t *fun = (wac_obj_fun_t*)obj;
			wac_gc_mark_obj(vm, (wac_obj_t*)fun->name);
			wac_gc_mark_valarr(vm, &fun->page.consts);
			break;
		}
		case WAC_OBJ_CLOSURE: {
			size_t i;
			wac_obj_closure_t *closure = (wac_obj_closure_t*)obj;
			wac_gc_mark_obj(vm, (wac_obj_t*)closure->fun);
			for (i = 0; i < closure->upvals_usize; ++i) {
				wac_gc_mark_obj(vm, (wac_obj_t*)closure->upvals[i]);
			}
			break;
		}
		case WAC_OBJ_UPVAL:
			wac_gc_mark_value(vm, ((wac_obj_upval_t*)obj)->closed);
			break;
	}
}

static void wac_gc_trace(wac_vm_t *vm) {
	while (vm->grays_usize > 0) {
		wac_gc_blacken(vm, vm->grays[--vm->grays_usize]);
	}
}

static void wac_gc_sweep(wac_state_t *state) {
	wac_obj_t *prev = NULL, *curr = state->vm.objs, *unreached;
	while (curr) {
		if (curr->isMarked) {
			curr->isMarked = false;
			prev = curr;
			curr = curr->next;
		} else {
			unreached = curr;
			curr = curr->next;
			if (prev) {
				prev->next = curr;
			} else {
				state->vm.objs = curr;
			}

			wac_obj_free(state, unreached);
		}
	}
}

static void wac_gc_collect(wac_state_t *state) {
#ifdef WAC_DEBUG_GC_LOG
	printf("[*] gc begin\n");
	size_t beforeGC = state->vm.mem_total;
#endif

	wac_gc_mark_roots(state);
	wac_gc_trace(&state->vm);

	size_t i;
	wac_table_entry_t *entry;
	for (i = 0; i < state->vm.strings.asize; ++i) {
		entry = &state->vm.strings.entries[i];
		if (entry->key && !entry->key->obj.isMarked) {
			wac_table_delete(&state->vm.strings, entry->key);
		}
	}

	wac_gc_sweep(state);

	state->vm.mem_nextGC = state->vm.mem_total * WAC_GC_GROW_MUL;

#ifdef WAC_DEBUG_GC_LOG
	printf("[*] gc end\n");
	printf("[*] gc collected %u bytes (from %u to %u)\n    next gc at %u\n", beforeGC - state->vm.mem_total, beforeGC, state->vm.mem_total, state->vm.mem_nextGC);
#endif
}

void* wac_realloc(wac_state_t *state, void *ptr, size_t oldSize, size_t newSize) {
	//coz size_t is unsigned
	//state->vm.mem_total += newSize - oldSize;

	if (newSize > oldSize) {
		state->vm.mem_total += newSize - oldSize;
#ifdef WAC_DEBUG_GC_STRESS
		wac_gc_collect(state);
#else
		if (state->vm.mem_total > state->vm.mem_nextGC) {
			wac_gc_collect(state);
		}
#endif
	} else {
		state->vm.mem_total -= oldSize - newSize;
	}

	if (newSize == 0) {
		free(ptr);
		return NULL;
	}
	
	void *result = NULL;
	if (!(result = realloc(ptr, newSize))) {
		fprintf(stderr, "[-] Failed to allocate memory\nwac_realloc(%p, %u, %u)\n", ptr, oldSize, newSize);
		exit(1);
	}
	return result;
}
