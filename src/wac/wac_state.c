#include <stdio.h>
#include <stdlib.h>

#include "wac_state.h"
#include "wac_common.h"

static wac_value_t wac_native_print(uint32_t argc, wac_value_t *argv) {
	wac_value_print(argv[0]);
	printf("\n");
	return WAC_VAL_NULL;
}

wac_state_t* wac_state_init() {
	wac_state_t *state = NULL;
	if (!(state = (wac_state_t*)malloc(sizeof(wac_state_t)))) {
		fprintf(stderr, "[-] Failed to allocate memory for wac state\n");
		exit(1);
	}
	state->compiler = NULL;
	state->classCompiler = NULL;

	wac_vm_init(state);

	wac_defineNativeFun(state, 1, "print", wac_native_print);
	return state;
}

void wac_state_free(wac_state_t *state) {
	wac_vm_free(state);
	free(state);
}
