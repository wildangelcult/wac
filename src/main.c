#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "wac/wac_common.h"
#include "wac/wac_state.h"

void repl(wac_state_t *state) {
	char line[4096];
	for (;;) {
		printf("> ");
		if (!fgets(line, sizeof(line), stdin)) break;
		wac_interpret(state, line);
	}
}

void runScript(wac_state_t *state, const char *filename) {
	long int size;
	FILE *fr = NULL;
	char *data = NULL;

	if (!(fr = fopen(filename, "rb"))) {
		fprintf(stderr, "Error opening '%s'\n", filename);
		exit(1);
	}

	fseek(fr, 0, SEEK_END);
	size = ftell(fr);
	rewind(fr);

	if (!(data = malloc(size + 1))) {
		fprintf(stderr, "Failed to malloc for data\n");
		exit(1);
	}

	if (fread(data, 1, size, fr) != size) {
		fprintf(stderr, "Failed to read\n");
		exit(1);
	}

	data[size] = '\0';

	wac_interpret(state, data);

	free(data);
}

wac_value_t native_clock(uint32_t argc, wac_value_t *argv) {
	return WAC_VAL_NUMBER((double)clock() / CLOCKS_PER_SEC);
}

int main(int argc, char *argv[]) {
	wac_state_t *W = wac_state_init();
	wac_defineNativeFun(W, 0, "clock", native_clock);
	if (argc == 2) {
		runScript(W, argv[1]);
	} else {
		repl(W);
	}
	/*
	runScript(W, "script.wac");
	*/
	wac_state_free(W);
	return 0;
}
