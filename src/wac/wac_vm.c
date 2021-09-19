#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "wac_state.h"
#include "wac_vm.h"
#include "wac_common.h"
#include "wac_memory.h"
#include "wac_value.h"
#include "wac_object.h"
#include "wac_compiler.h"

#ifdef WAC_DEBUG_TRACE_EXEC
#include "wac_debug.h"
#endif

static wac_interpretResult_t wac_vm_run(wac_state_t *state);

void wac_defineNativeFun(wac_state_t *state, uint32_t arity, const char *name, wac_native_fun_t fun) {
	wac_vm_push(&state->vm, WAC_VAL_OBJ(wac_obj_string_copy(state, name, strlen(name))));
	wac_vm_push(&state->vm, WAC_VAL_OBJ(wac_obj_native_init(state, arity, WAC_OBJ_AS_STRING(state->vm.stack[0]), fun)));
	wac_table_set(state, &state->vm.globals, WAC_OBJ_AS_STRING(state->vm.stack[0]), state->vm.stack[1]);
	wac_vm_pop(&state->vm);
	wac_vm_pop(&state->vm);
}

static void wac_vm_stack_reset(wac_vm_t *vm) {
	vm->sp = vm->stack;
	vm->frames_usize = 0;
	vm->openUpvals = NULL;
}

static void wac_vm_error(wac_vm_t *vm, const char *fmt, ...) {
	size_t i;
	wac_obj_fun_t *fun;
	wac_frame_t *frame;

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");

	fprintf(stderr, "Error in:\n");
	for (i = vm->frames_usize - 1; i != ((size_t)-1); --i) {
		frame = &vm->frames[i];
		fun = frame->closure->fun;
		fprintf(stderr, "[%u] ", fun->page.lines[frame->ip - fun->page.code - 1]);

		if (fun->name) {
			fprintf(stderr, "%s()\n", fun->name->buf);
		} else {
			fprintf(stderr, "<script>\n");
		}
	}
	
	wac_vm_stack_reset(vm);
}

void wac_vm_init(wac_state_t *state) {
	wac_vm_t *vm = &state->vm;

	vm->stack_asize = WAC_ARRAY_DEFAULT_SIZE;
	vm->stack = WAC_ARRAY_INIT_NOGC(wac_value_t, vm->stack_asize);

	wac_vm_stack_reset(vm);

	vm->grays_usize = 0;
	vm->grays_asize = WAC_ARRAY_DEFAULT_SIZE;
	vm->grays = WAC_ARRAY_INIT_NOGC(wac_obj_t*, vm->grays_asize);

	vm->objs = NULL;

	vm->mem_total = 0;
	vm->mem_nextGC = 1024 * 1024;

	//vm ready, you can use wac_realloc

	//coz the gc loops over vm->strings
	vm->strings.asize = 0;
	wac_table_init(state, &vm->globals);
	wac_table_init(state, &vm->strings);

	vm->frames_asize = WAC_ARRAY_DEFAULT_SIZE;
	vm->frames = WAC_ARRAY_INIT(state, wac_frame_t, vm->frames_asize);
}

static wac_frame_t* wac_vm_newFrame(wac_state_t *state) {
	wac_vm_t *vm = &state->vm;

	if (vm->frames_asize <= vm->frames_usize) {
		size_t oldSize = vm->frames_asize;
		vm->frames_asize *= WAC_ARRAY_GROW_MUL;
		vm->frames = WAC_ARRAY_GROW(state, wac_frame_t, vm->frames, oldSize, vm->frames_asize);
	}
	return &vm->frames[vm->frames_usize++];
}


void wac_vm_push(wac_vm_t *vm, wac_value_t value) {
	size_t stackIndex = vm->sp - vm->stack;
	if (vm->stack_asize <= stackIndex) {
		size_t newSize = vm->stack_asize * WAC_ARRAY_GROW_MUL, i, j;
		wac_obj_upval_t *upval;
		wac_value_t *newStack = WAC_ARRAY_INIT_NOGC(wac_value_t, newSize);

		for (i = 0; i < stackIndex; ++i) {
			newStack[i] = vm->stack[i];
		}

		for (i = 0; i < vm->frames_usize; ++i) {
			for (j = 0; j < vm->frames[i].closure->upvals_usize; ++j) {
				upval = vm->frames[i].closure->upvals[j];
				if (upval->loc != &upval->closed) {
					upval->loc = &newStack[upval->loc - vm->stack];
				}
			}
			vm->frames[i].bp = &newStack[vm->frames[i].bp - vm->stack];
		}

		free(vm->stack);
		vm->stack = newStack;
		vm->stack_asize = newSize;

		vm->sp = &vm->stack[stackIndex];
	}
	*vm->sp++ = value;
}

wac_value_t wac_vm_pop(wac_vm_t *vm) {
#ifdef WAC_DEBUG_STACK_CHECK
	if (vm->sp == vm->stack) {
		fprintf(stderr, "[-] Error: stack underflow\n");
	}
#endif
	return *--vm->sp;
}

static wac_value_t wac_vm_peek(wac_vm_t *vm, size_t dist) {
	return vm->sp[-1 - dist];
}

static void wac_vm_concat(wac_state_t *state) {
	wac_vm_t *vm = &state->vm;

	wac_obj_string_t *b = WAC_OBJ_AS_STRING(wac_vm_peek(vm, 0));
	wac_obj_string_t *a = WAC_OBJ_AS_STRING(wac_vm_peek(vm, 1));

	size_t len = a->len + b->len;
	char *buf = WAC_ARRAY_INIT(state, char, len + 1);
	memcpy(buf, a->buf, a->len);
	memcpy(buf + a->len, b->buf, b->len);
	buf[len] = '\0';

	wac_obj_string_t *result = wac_obj_string_take(state, buf, len);
	wac_vm_pop(vm);
	wac_vm_pop(vm);
	wac_vm_push(vm, WAC_VAL_OBJ(result));
}

static bool wac_vm_call(wac_state_t *state, wac_obj_closure_t *closure, uint32_t argc) {
	if (closure->fun->arity != argc) {
		wac_vm_error(&state->vm, "Expected %u arguments, but got %u", closure->fun->arity, argc);
		return false;
	}
	wac_frame_t *newFrame = wac_vm_newFrame(state);
	newFrame->closure = closure;
	newFrame->ip = closure->fun->page.code;
	newFrame->bp = state->vm.sp - argc - 1;
	return true;
}

static bool wac_vm_call_value(wac_state_t *state, wac_value_t callee, uint32_t argc) {
	if (WAC_VAL_IS_OBJ(callee)) {
		switch (WAC_OBJ_TYPE(callee)) {
			case WAC_OBJ_NATIVE: {
				wac_obj_native_t *native = WAC_OBJ_AS_NATIVE(callee);
				if (native->arity != argc) {
					wac_vm_error(&state->vm, "Expected %u arguments, but got %u", native->arity, argc);
					return false;
				}
				wac_value_t result = native->fun(argc, state->vm.sp - argc);
				state->vm.sp -= argc + 1;
				wac_vm_push(&state->vm, result);
				return true;
			}
			case WAC_OBJ_CLOSURE:
				return wac_vm_call(state, WAC_OBJ_AS_CLOSURE(callee), argc);
			default:
				break;
		}
	}
	wac_vm_error(&state->vm, "You can only call functions and classes");
	return false;
}

static wac_obj_upval_t* wac_vm_captureUpval(wac_state_t *state, wac_value_t *local) {
	wac_obj_upval_t *prev = NULL, *curr = state->vm.openUpvals;

	while (curr && curr->loc > local) {
		prev = curr;
		curr = curr->next;
	}

	if (curr && curr->loc == local) {
		return curr;
	}


	wac_obj_upval_t *upval = wac_obj_upval_init(state, local);

	upval->next = curr;
	if (prev) {
		prev->next = upval;
	} else {
		state->vm.openUpvals = upval;
	}
	return upval;
}

static void wac_vm_closeUpvals(wac_vm_t *vm, wac_value_t *last) {
	wac_obj_upval_t *upval;
	while (vm->openUpvals && vm->openUpvals->loc >= last) {
		upval = vm->openUpvals;
		upval->closed = *upval->loc;
		upval->loc = &upval->closed;
		vm->openUpvals = upval->next;
	}
}

wac_interpretResult_t wac_interpret(wac_state_t *state, const char *src) {
	wac_obj_fun_t *fun = wac_compiler_compile(state, src);
	if (!fun) return WAC_INTERPRET_COMPILE_ERROR;
	
	wac_vm_push(&state->vm, WAC_VAL_OBJ(fun));
	wac_obj_closure_t *closure = wac_obj_closure_init(state, fun);
	wac_vm_pop(&state->vm);
	wac_vm_push(&state->vm, WAC_VAL_OBJ(closure));
	wac_vm_call(state, closure, 0);

	return wac_vm_run(state);
}

static wac_interpretResult_t wac_vm_run(wac_state_t *state) {
#define WAC_READ_BYTE() (*frame->ip++)
#define WAC_READ_4_BYTES() ((WAC_READ_BYTE() << 24) | (WAC_READ_BYTE() << 16) | (WAC_READ_BYTE() << 8) | WAC_READ_BYTE())
#define WAC_READ_CONST() (frame->closure->fun->page.consts.values[WAC_READ_4_BYTES()])
#define WAC_READ_STRING() (WAC_OBJ_AS_STRING(WAC_READ_CONST()))
#define WAC_BIN_OP(valueType, op) \
	do {\
		if (!WAC_VAL_IS_NUMBER(wac_vm_peek(vm, 0)) || !WAC_VAL_IS_NUMBER(wac_vm_peek(vm, 1))) {\
			wac_vm_error(vm, "Operands must be numbers");\
			return WAC_INTERPRET_RUNTIME_ERROR;\
		}\
		double b = WAC_VAL_AS_NUMBER(wac_vm_pop(vm));\
		double a = WAC_VAL_AS_NUMBER(wac_vm_pop(vm));\
		wac_vm_push(vm, valueType(a op b));\
	} while (false)

	wac_vm_t *vm = &state->vm;
	wac_frame_t *frame = &vm->frames[vm->frames_usize - 1];

	uint8_t inst;
	for (;;) {
#ifdef WAC_DEBUG_TRACE_EXEC
		for (wac_value_t *value = vm->stack; value < vm->sp; ++value) {
			printf("[ ");
			wac_value_print(*value);
			printf(" ]");
		}
		printf("\n");
		wac_inst_disass(&frame->closure->fun->page, frame->ip - frame->closure->fun->page.code);
#endif
		switch (inst = WAC_READ_BYTE()) {
			case WAC_OP_CONST:
				wac_vm_push(vm, WAC_READ_CONST());
				break;
			case WAC_OP_NULL:
				wac_vm_push(vm, WAC_VAL_NULL);
				break;
			case WAC_OP_TRUE:
				wac_vm_push(vm, WAC_VAL_BOOL(true));
				break;
			case WAC_OP_FALSE:
				wac_vm_push(vm, WAC_VAL_BOOL(false));
				break;
			case WAC_OP_CLOSURE: {
				size_t i;
				uint8_t isLocal;
				uint32_t index;
				wac_obj_closure_t *closure = wac_obj_closure_init(state, WAC_OBJ_AS_FUN(WAC_READ_CONST()));
				wac_vm_push(vm, WAC_VAL_OBJ(closure));

				for (i = 0; i < closure->upvals_usize; ++i) {
					isLocal = WAC_READ_BYTE();
					index = WAC_READ_4_BYTES();

					if (isLocal) {
						closure->upvals[i] = wac_vm_captureUpval(state, frame->bp + index);
					} else {
						closure->upvals[i] = frame->closure->upvals[index];
					}
				}
				break;
			}
			case WAC_OP_POP:
				wac_vm_pop(vm);
				break;
			case WAC_OP_POPN:
#ifdef WAC_DEBUG_STACK_CHECK
			{
				uint32_t n = WAC_READ_4_BYTES();
				if (vm->sp - vm->stack < n) {
					fprintf(stderr, "[-] Error: stack underflow\n");
				}
				vm->sp -= n;
			}
#else
				vm->sp -= WAC_READ_4_BYTES();
#endif
				break;
			case WAC_OP_GET_LOCAL:
				wac_vm_push(vm, frame->bp[WAC_READ_4_BYTES()]);
				break;
			case WAC_OP_SET_LOCAL:
				frame->bp[WAC_READ_4_BYTES()] = wac_vm_peek(vm, 0);
				break;
			case WAC_OP_GET_UPVAL:
				wac_vm_push(vm, *frame->closure->upvals[WAC_READ_4_BYTES()]->loc);
				break;
			case WAC_OP_SET_UPVAL:
				*frame->closure->upvals[WAC_READ_4_BYTES()]->loc = wac_vm_peek(vm, 0);
				break;
			case WAC_OP_GET_GLOBAL: {
				wac_obj_string_t *name = WAC_READ_STRING();
				wac_value_t value;

				if (!wac_table_get(&vm->globals, name, &value)) {
					wac_vm_error(vm, "Undefined variable '%s'", name->buf);
					return WAC_INTERPRET_RUNTIME_ERROR;
				}

				wac_vm_push(vm, value);
				break;
			}
			case WAC_OP_SET_GLOBAL: {
				wac_obj_string_t *name = WAC_READ_STRING();
				if (wac_table_set(state, &vm->globals, name, wac_vm_peek(vm, 0))) {
					wac_table_delete(&vm->globals, name);
					wac_vm_error(vm, "Undefined variable '%s'", name->buf);
					return WAC_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case WAC_OP_CLOSE_UPVAL:
				wac_vm_closeUpvals(vm, vm->sp - 1);
				wac_vm_pop(vm);
				break;
			case WAC_OP_DEFINE_GLOBAL:
				wac_table_set(state, &vm->globals, WAC_READ_STRING(), wac_vm_peek(&state->vm, 0));
				wac_vm_pop(vm);
				break;
			case WAC_OP_NOT:
				//wac_vm_push(vm, WAC_VAL_BOOL(wac_value_falsey(wac_vm_pop(vm))));
				vm->sp[-1] = WAC_VAL_BOOL(wac_value_falsey(vm->sp[-1]));
				break;
			case WAC_OP_EQUAL: {
				wac_value_t a = wac_vm_pop(vm);
				wac_value_t b = wac_vm_pop(vm);
				wac_vm_push(vm, WAC_VAL_BOOL(wac_value_equal(a, b)));
				break;
			}
			case WAC_OP_GREATER:
				WAC_BIN_OP(WAC_VAL_BOOL, >);
				break;
			case WAC_OP_LESS:
				WAC_BIN_OP(WAC_VAL_BOOL, <);
				break;
			case WAC_OP_NEG:
				if (!WAC_VAL_IS_NUMBER(wac_vm_peek(vm, 0))) {
					wac_vm_error(vm, "Operand must be a number");
					return WAC_INTERPRET_RUNTIME_ERROR;
				}
				//wac_vm_push(vm, WAC_VAL_NUMBER(-WAC_VAL_AS_NUMBER(wac_vm_pop(vm))));
				vm->sp[-1] = WAC_VAL_NUMBER(-WAC_VAL_AS_NUMBER(vm->sp[-1]));
				break;
			case WAC_OP_ADD: {
				if (WAC_OBJ_IS_STRING(wac_vm_peek(vm, 0)) && WAC_OBJ_IS_STRING(wac_vm_peek(vm, 0))) {
					wac_vm_concat(state);
				} else if (WAC_VAL_IS_NUMBER(wac_vm_peek(vm, 0)) && WAC_VAL_IS_NUMBER(wac_vm_peek(vm, 0))) {
					double b = WAC_VAL_AS_NUMBER(wac_vm_pop(vm));
					double a = WAC_VAL_AS_NUMBER(wac_vm_pop(vm));
					wac_vm_push(vm, WAC_VAL_NUMBER(a + b));
				} else {
					wac_vm_error(vm, "Operands must be two numbers or two strings");
				}
				break;
			}
			case WAC_OP_SUB:
				WAC_BIN_OP(WAC_VAL_NUMBER, -);
				break;
			case WAC_OP_MUL:
				WAC_BIN_OP(WAC_VAL_NUMBER, *);
				break;
			case WAC_OP_DIV:
				WAC_BIN_OP(WAC_VAL_NUMBER, /);
				break;
			case WAC_OP_JMP_FORW: {
				uint32_t address = WAC_READ_4_BYTES();
				frame->ip += address;
				break;
			}
			case WAC_OP_JMP_BACK: {
				uint32_t address = WAC_READ_4_BYTES();
				frame->ip -= address;
				break;
			}
			case WAC_OP_JMP_TRUE: {
				uint32_t address = WAC_READ_4_BYTES();
				if (!wac_value_falsey(wac_vm_peek(vm, 0))) frame->ip += address;
				break;
			}
			case WAC_OP_JMP_FALSE: {
				uint32_t address = WAC_READ_4_BYTES();
				if (wac_value_falsey(wac_vm_peek(vm, 0))) frame->ip += address;
				break;
			}
			case WAC_OP_CALL: {
				uint32_t argc = WAC_READ_4_BYTES();
				if (!wac_vm_call_value(state, wac_vm_peek(vm, argc), argc)) return WAC_INTERPRET_RUNTIME_ERROR;
				frame = &vm->frames[vm->frames_usize - 1];
				break;
			}
			case WAC_OP_RET: {
				wac_value_t result = wac_vm_pop(vm);
				wac_vm_closeUpvals(vm, frame->bp);
				--vm->frames_usize;
				if (vm->frames_usize == 0) {
					wac_vm_pop(vm);
					return WAC_INTERPRET_OK;
				}
				vm->sp = frame->bp;
				wac_vm_push(vm, result);
				frame = &vm->frames[vm->frames_usize - 1];
				break;
			}
		}
	}
}

static void wac_vm_objs_free(wac_state_t *state) {
	wac_obj_t *curr = state->vm.objs, *next;
	while (curr) {
		next = curr->next;
		wac_obj_free(state, curr);
		curr = next;
	}
}

void wac_vm_free(wac_state_t *state) {
	wac_vm_t *vm = &state->vm;

	WAC_ARRAY_FREE(state, wac_frame_t, vm->frames, vm->frames_asize);
	free(vm->stack);
	free(vm->grays);

	vm->stack_asize = 0;
	vm->sp = NULL;
	vm->stack = NULL;

	wac_vm_objs_free(state);
	wac_table_free(state, &vm->globals);
	wac_table_free(state, &vm->strings);
}
