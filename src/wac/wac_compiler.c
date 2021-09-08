#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wac_state.h"
#include "wac_value.h"
#include "wac_object.h"
#include "wac_compiler.h"
#include "wac_memory.h"

#ifdef WAC_DEBUG_PRINT_CODE
#include "wac_debug.h"
#endif

#define INVALID_UINT ((unsigned int)(-1))
#define INVALID_UINT32 ((uint32_t)(-1))
#define INVALID_SIZE ((size_t)(-1))

//forward decl
static void wac_parser_decl(wac_state_t *state);
static void wac_parser_statement(wac_state_t *state);
static void wac_parser_decl_var(wac_state_t *state);

typedef enum wac_parser_prec_e {
	WAC_PREC_NONE,
	WAC_PREC_ASSIGN,	// =
	WAC_PREC_OR,		// ||
	WAC_PREC_AND,    	// &&
	WAC_PREC_EQUAL,  	// == !=
	WAC_PREC_COMPARE,	// < > <= >=
	WAC_PREC_TERM,   	// + -
	WAC_PREC_FACT,   	// * /
	WAC_PREC_UNARY,  	// ! -
	WAC_PREC_CALL,   	// . ()
	WAC_PREC_PRIMARY
} wac_parser_prec_t;

typedef void (*wac_parser_fun_t)(wac_state_t*, bool);
typedef struct wac_parser_rule_s {
	wac_parser_fun_t prefix, infix;
	wac_parser_prec_t prec;
} wac_parser_rule_t;

//forward decl for parser rules
static void wac_parser_group(wac_state_t *state, bool canAssign);
static void wac_parser_unary(wac_state_t *state, bool canAssign);
static void wac_parser_binary(wac_state_t *state, bool canAssign);
static void wac_parser_number(wac_state_t *state, bool canAssign);
static void wac_parser_literal(wac_state_t *state, bool canAssign);
static void wac_parser_string(wac_state_t *state, bool canAssign);
static void wac_parser_variable(wac_state_t *state, bool canAssign);
static void wac_parser_and(wac_state_t *state, bool canAssign);
static void wac_parser_or(wac_state_t *state, bool canAssign);
static void wac_parser_call(wac_state_t *state, bool canAssign);

static wac_parser_rule_t wac_parser_rules[] = {
	[WAC_TOKEN_LPAREN]		= {wac_parser_group,	wac_parser_call,	WAC_PREC_CALL},
	[WAC_TOKEN_RPAREN]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_LCURLY]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_RCURLY]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_COMMA]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_DOT]			= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_PLUS]		= {wac_parser_unary,	wac_parser_binary,	WAC_PREC_TERM},
	[WAC_TOKEN_MINUS]		= {wac_parser_unary,	wac_parser_binary,	WAC_PREC_TERM},
	[WAC_TOKEN_STAR]		= {NULL,		wac_parser_binary,	WAC_PREC_FACT},
	[WAC_TOKEN_SLASH]		= {NULL,		wac_parser_binary,	WAC_PREC_FACT},
	[WAC_TOKEN_SEMICOLON]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_BANG]		= {wac_parser_unary,	NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_BANG_EQUAL]		= {NULL,		wac_parser_binary,	WAC_PREC_EQUAL},
	[WAC_TOKEN_EQUAL]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_EQUAL_EQUAL]		= {NULL,		wac_parser_binary,	WAC_PREC_EQUAL},
	[WAC_TOKEN_GREATER]		= {NULL,		wac_parser_binary,	WAC_PREC_COMPARE},
	[WAC_TOKEN_GREATER_EQUAL]	= {NULL,		wac_parser_binary,	WAC_PREC_COMPARE},
	[WAC_TOKEN_LESS]		= {NULL,		wac_parser_binary,	WAC_PREC_COMPARE},
	[WAC_TOKEN_LESS_EQUAL]		= {NULL,		wac_parser_binary,	WAC_PREC_COMPARE},
	[WAC_TOKEN_AMPER_AMPER]		= {NULL,		wac_parser_and,		WAC_PREC_AND},
	[WAC_TOKEN_BAR_BAR]		= {NULL,		wac_parser_or,		WAC_PREC_OR},
	[WAC_TOKEN_ID]			= {wac_parser_variable,	NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_STRING]		= {wac_parser_string,	NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_NUMBER]		= {wac_parser_number,	NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_VAR]			= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_CLASS]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_THIS]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_SUPER]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_FUN]			= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_RETURN]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_IF]			= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_ELSE]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_TRUE]		= {wac_parser_literal,	NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_FALSE]		= {wac_parser_literal,	NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_FOR]			= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_WHILE]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_NULL]		= {wac_parser_literal,	NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_ERROR]		= {NULL,		NULL,			WAC_PREC_NONE},
	[WAC_TOKEN_EOF]			= {NULL,		NULL,			WAC_PREC_NONE},
};

static void wac_parser_errorAt(wac_parser_t *parser, wac_token_t *token, const char *msg) {
	if (parser->panic) return;
	parser->panic = true;
	fprintf(stderr, "[%u] Error", token->line);
	
	if (token->type == WAC_TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == WAC_TOKEN_ERROR) {
	} else {
		fprintf(stderr, " at '%.*s'", token->len, token->start);
	}

	fprintf(stderr, ": %s\n", msg);
	parser->error = true;
}

static void wac_parser_errorAtCurr(wac_parser_t *parser, const char *msg) {
	wac_parser_errorAt(parser, &parser->curr, msg);
}

static void wac_parser_error(wac_parser_t *parser, const char *msg) {
	wac_parser_errorAt(parser, &parser->prev, msg);
}

static void wac_parser_advance(wac_state_t *state) {
	state->parser.prev = state->parser.curr;

	for (;;) {
		state->parser.curr = wac_scanner_token_next(&state->scanner);
		if (state->parser.curr.type != WAC_TOKEN_ERROR) break;
		wac_parser_errorAtCurr(&state->parser, state->parser.curr.start);
	}
}

static void wac_parser_eat(wac_state_t *state, wac_token_type_t type, const char *msg) {
	if (state->parser.curr.type == type) {
		wac_parser_advance(state);
		return;
	}

	wac_parser_errorAtCurr(&state->parser, msg);
}

static void wac_compiler_emit_byte(wac_state_t *state, uint8_t byte) {
	wac_page_write_byte(state, &state->compiler->fun->page, byte, state->parser.prev.line);
}

static void wac_compiler_emit_2bytes(wac_state_t *state, uint8_t byte1, uint8_t byte2) {
	wac_compiler_emit_byte(state, byte1);
	wac_compiler_emit_byte(state, byte2);
}

static void wac_compiler_emit_4bytes(wac_state_t *state, uint32_t bytes) {
	wac_page_write_4bytes(state, &state->compiler->fun->page, bytes, state->parser.prev.line);
}

static void wac_compiler_emit_5bytes(wac_state_t *state, uint8_t byte, uint32_t bytes) {
	wac_compiler_emit_byte(state, byte);
	wac_compiler_emit_4bytes(state, bytes);
}

static void wac_compiler_emit_ret(wac_state_t *state) {
	wac_compiler_emit_2bytes(state, WAC_OP_NULL, WAC_OP_RET);
}

static void wac_compiler_emit_const(wac_state_t *state, uint8_t op, wac_value_t value) {
	wac_compiler_emit_5bytes(state, op, wac_page_addConst(state, &state->compiler->fun->page, value));
}

static size_t wac_compiler_emit_jmp_forw(wac_state_t *state, uint8_t inst) {
	wac_compiler_emit_5bytes(state, inst, INVALID_UINT32);
	return state->compiler->fun->page.usize - 4;
}

static void wac_compiler_patchJmp(wac_page_t *page, size_t address) {
	size_t jmpAddr = page->usize - address - 4;
	page->code[address    ] = (jmpAddr & 0xFF000000) >> 24;
	page->code[address + 1] = (jmpAddr & 0x00FF0000) >> 16;
	page->code[address + 2] = (jmpAddr & 0x0000FF00) >> 8;
	page->code[address + 3] = (jmpAddr & 0x000000FF);
}

static void wac_compiler_emit_jmp_back(wac_state_t *state, size_t address) {
	wac_compiler_emit_5bytes(state, WAC_OP_JMP_BACK, state->compiler->fun->page.usize - address + 5);
}

static void wac_compiler_init(wac_state_t *state, wac_compiler_t *compiler, wac_fun_type_t type) {
	wac_local_t *local;

	compiler->prev = state->compiler;
	compiler->fun = NULL;
	compiler->type = type;

	compiler->locals_asize = WAC_ARRAY_DEFAULT_SIZE;
	compiler->locals_usize = 0;
	compiler->locals = WAC_ARRAY_INIT(state, wac_local_t, compiler->locals_asize);

	compiler->scopeDepth = 0;
	compiler->fun = wac_obj_fun_init(state);
	state->compiler = compiler;

	if (type == WAC_FUN_TYPE_SCRIPT) {
		compiler->upvals_asize = 0;
		compiler->upvals = NULL;
	} else {
		compiler->upvals_asize = WAC_ARRAY_DEFAULT_SIZE;
		compiler->upvals = WAC_ARRAY_INIT(state, wac_upval_t, compiler->upvals_asize);
		state->compiler->fun->name = wac_obj_string_copy(state, state->parser.prev.start, state->parser.prev.len);
	}

	local = &state->compiler->locals[state->compiler->locals_usize++];
	local->depth = 0;
	local->isCaptured = false;
	local->name.start = "";
	local->name.len = 0;
}

static wac_obj_fun_t* wac_compiler_end(wac_state_t *state) {
	wac_compiler_emit_ret(state);
	wac_obj_fun_t *fun = state->compiler->fun;
#ifdef WAC_DEBUG_PRINT_CODE
	if (!state->parser.error) {
		wac_page_disass(&state->compiler->fun->page, fun->name ? fun->name->buf : "<script>");
	}
#endif
	WAC_ARRAY_FREE(state, wac_local_t, state->compiler->locals, state->compiler->locals_asize);
	state->compiler->locals_asize = 0;
	state->compiler->locals_usize = 0;
	state->compiler->locals = NULL;

	state->compiler = state->compiler->prev;
	return fun;
}

static bool wac_parser_check(wac_parser_t *parser, wac_token_type_t type) {
	return parser->curr.type == type;
}

static bool wac_parser_match(wac_state_t *state, wac_token_type_t type) {
	if (!wac_parser_check(&state->parser, type)) return false;
	wac_parser_advance(state);
	return true;
}

static void wac_parser_prec(wac_state_t *state, wac_parser_prec_t prec) {
	wac_parser_advance(state);
	wac_parser_fun_t prefix = wac_parser_rules[state->parser.prev.type].prefix;
	if (!prefix) {
		wac_parser_error(&state->parser, "Expected expression");
		return;
	}

	bool canAssign = prec <= WAC_PREC_ASSIGN;
	prefix(state, canAssign);

	while (prec <= wac_parser_rules[state->parser.curr.type].prec) {
		wac_parser_advance(state);
		wac_parser_rules[state->parser.prev.type].infix(state, canAssign);
	}

	if (canAssign && wac_parser_match(state, WAC_TOKEN_EQUAL)) {
		wac_parser_error(&state->parser, "Invalid assignment target");
	}
}

static void wac_parser_expr(wac_state_t *state) {
	wac_parser_prec(state, WAC_PREC_ASSIGN);
}

static void wac_parser_number(wac_state_t *state, bool canAssign) {
	wac_compiler_emit_const(state, WAC_OP_CONST, WAC_VAL_NUMBER(strtod(state->parser.prev.start, NULL)));
}

static void wac_parser_group(wac_state_t *state, bool canAssign) {
	wac_parser_expr(state);
	wac_parser_eat(state, WAC_TOKEN_RPAREN, "Expected ')' after expresion");
}

static void wac_parser_unary(wac_state_t *state, bool canAssign) {
	wac_token_type_t type = state->parser.prev.type;

	wac_parser_prec(state, WAC_PREC_UNARY);

	switch (type) {
		case WAC_TOKEN_PLUS: break;
		case WAC_TOKEN_MINUS: wac_compiler_emit_byte(state, WAC_OP_NEG); break;
		case WAC_TOKEN_BANG: wac_compiler_emit_byte(state, WAC_OP_NOT); break;
		default: return;
	}
}

static void wac_parser_binary(wac_state_t *state, bool canAssign) {
	wac_token_type_t type = state->parser.prev.type;
	wac_parser_prec(state, (wac_parser_prec_t)(wac_parser_rules[type].prec + 1));

	switch (type) {
		case WAC_TOKEN_PLUS: wac_compiler_emit_byte(state, WAC_OP_ADD); break;
		case WAC_TOKEN_MINUS: wac_compiler_emit_byte(state, WAC_OP_SUB); break;
		case WAC_TOKEN_STAR: wac_compiler_emit_byte(state, WAC_OP_MUL); break;
		case WAC_TOKEN_SLASH: wac_compiler_emit_byte(state, WAC_OP_DIV); break;
		case WAC_TOKEN_BANG_EQUAL: wac_compiler_emit_2bytes(state, WAC_OP_EQUAL, WAC_OP_NOT); break;
		case WAC_TOKEN_EQUAL_EQUAL: wac_compiler_emit_byte(state, WAC_OP_EQUAL); break;
		case WAC_TOKEN_GREATER: wac_compiler_emit_byte(state, WAC_OP_GREATER); break;
		case WAC_TOKEN_GREATER_EQUAL: wac_compiler_emit_2bytes(state, WAC_OP_LESS, WAC_OP_NOT); break;
		case WAC_TOKEN_LESS: wac_compiler_emit_byte(state, WAC_OP_LESS); break;
		case WAC_TOKEN_LESS_EQUAL: wac_compiler_emit_2bytes(state, WAC_OP_GREATER, WAC_OP_NOT); break;
		default: return;
	}
}

static void wac_parser_literal(wac_state_t *state, bool canAssign) {
	switch (state->parser.prev.type) {
		case WAC_TOKEN_NULL: wac_compiler_emit_byte(state, WAC_OP_NULL); break;
		case WAC_TOKEN_TRUE: wac_compiler_emit_byte(state, WAC_OP_TRUE); break;
		case WAC_TOKEN_FALSE: wac_compiler_emit_byte(state, WAC_OP_FALSE); break;
		default: return;
	}
}

static void wac_parser_string(wac_state_t *state, bool canAssign) {
	wac_compiler_emit_const(state, WAC_OP_CONST, WAC_VAL_OBJ(wac_obj_string_copy(state, state->parser.prev.start + 1, state->parser.prev.len - 2)));
}

static void wac_parser_statement_expr(wac_state_t *state) {
	wac_parser_expr(state);
	wac_parser_eat(state, WAC_TOKEN_SEMICOLON, "Expected ';' after expression");
	wac_compiler_emit_byte(state, WAC_OP_POP);
}

static void wac_parser_statement_block(wac_state_t *state) {
	while (!wac_parser_check(&state->parser, WAC_TOKEN_RCURLY) && !wac_parser_check(&state->parser, WAC_TOKEN_EOF)) {
		wac_parser_decl(state);
	}

	wac_parser_eat(state, WAC_TOKEN_RCURLY, "Expected '}' after block");
}

static void wac_parser_statement_if(wac_state_t *state) {
	size_t ifJmp, elseJmp;

	wac_parser_eat(state, WAC_TOKEN_LPAREN, "Expected '(' after if");
	wac_parser_expr(state);
	wac_parser_eat(state, WAC_TOKEN_RPAREN, "Expected ')' after condition");

	ifJmp = wac_compiler_emit_jmp_forw(state, WAC_OP_JMP_FALSE);
	wac_compiler_emit_byte(state, WAC_OP_POP);
	wac_parser_statement(state);

	elseJmp = wac_compiler_emit_jmp_forw(state, WAC_OP_JMP_FORW);

	wac_compiler_patchJmp(&state->compiler->fun->page, ifJmp);
	wac_compiler_emit_byte(state, WAC_OP_POP);

	if (wac_parser_match(state, WAC_TOKEN_ELSE)) wac_parser_statement(state);
	wac_compiler_patchJmp(&state->compiler->fun->page, elseJmp);
}

static void wac_parser_statement_while(wac_state_t* state) {
	size_t exitJmp, loopJmp = state->compiler->fun->page.usize;

	wac_parser_eat(state, WAC_TOKEN_LPAREN, "Expected '(' after while");
	wac_parser_expr(state);
	wac_parser_eat(state, WAC_TOKEN_RPAREN, "Expected ')' after condition");

	exitJmp = wac_compiler_emit_jmp_forw(state, WAC_OP_JMP_FALSE);
	wac_compiler_emit_byte(state, WAC_OP_POP);
	wac_parser_statement(state);
	wac_compiler_emit_jmp_back(state, loopJmp);

	wac_compiler_patchJmp(&state->compiler->fun->page, exitJmp);
	wac_compiler_emit_byte(state, WAC_OP_POP);
}

static void wac_compiler_scope_begin(wac_state_t *state) {
	++state->compiler->scopeDepth;
}

static void wac_compiler_scope_end(wac_state_t *state) {
	uint32_t numLocals = 0;
	--state->compiler->scopeDepth;

	while (state->compiler->locals_usize > 0 && state->compiler->locals[state->compiler->locals_usize - 1].depth > state->compiler->scopeDepth) {
		if (state->compiler->locals[state->compiler->locals_usize - 1].isCaptured) {
			if (numLocals > 0) {
				wac_compiler_emit_5bytes(state, WAC_OP_POPN, numLocals);
				numLocals = 0;
			}
			wac_compiler_emit_byte(state, WAC_OP_CLOSE_UPVAL);
		} else {
			++numLocals;
		}
		--state->compiler->locals_usize;
	}

	if (numLocals > 0) {
		wac_compiler_emit_5bytes(state, WAC_OP_POPN, numLocals);
	}
}

static void wac_parser_statement_for(wac_state_t *state) {
	size_t loopJmp, exitJmp;

	wac_compiler_scope_begin(state);

	wac_parser_eat(state, WAC_TOKEN_LPAREN, "Expected '(' after for");
	if (wac_parser_match(state, WAC_TOKEN_SEMICOLON)) {
	} else if (wac_parser_match(state, WAC_TOKEN_VAR)) {
		wac_parser_decl_var(state);
	} else {
		wac_parser_statement_expr(state);
	}

	loopJmp = state->compiler->fun->page.usize;
	exitJmp = INVALID_SIZE;
	if (!wac_parser_match(state, WAC_TOKEN_SEMICOLON)) {
		wac_parser_expr(state);
		wac_parser_eat(state, WAC_TOKEN_SEMICOLON, "Expected ';' after for loop condition");
		exitJmp = wac_compiler_emit_jmp_forw(state, WAC_OP_JMP_FALSE);
		wac_compiler_emit_byte(state, WAC_OP_POP);
	}

	if (!wac_parser_match(state, WAC_TOKEN_RPAREN)) {
		size_t bodyJmp = wac_compiler_emit_jmp_forw(state, WAC_OP_JMP_FORW);
		size_t incJmp = state->compiler->fun->page.usize;
		wac_parser_expr(state);
		wac_compiler_emit_byte(state, WAC_OP_POP);
		wac_parser_eat(state, WAC_TOKEN_RPAREN, "Expected ')' after for clauses");

		wac_compiler_emit_jmp_back(state, loopJmp);
		loopJmp = incJmp;
		wac_compiler_patchJmp(&state->compiler->fun->page, bodyJmp);
	}
	
	wac_parser_statement(state);
	wac_compiler_emit_jmp_back(state, loopJmp);

	if (exitJmp != INVALID_SIZE) {
		wac_compiler_patchJmp(&state->compiler->fun->page, exitJmp);
		wac_compiler_emit_byte(state, WAC_OP_POP);
	}

	wac_compiler_scope_end(state);
}

static void wac_parser_statement_ret(wac_state_t *state) {
	if (wac_parser_match(state, WAC_TOKEN_SEMICOLON)) {
		wac_compiler_emit_ret(state);
	} else {
		wac_parser_expr(state);
		wac_parser_eat(state, WAC_TOKEN_SEMICOLON, "Expected ';' after return value");
		wac_compiler_emit_byte(state, WAC_OP_RET);
	}
}

static void wac_parser_statement(wac_state_t *state) {
	if (wac_parser_match(state, WAC_TOKEN_LCURLY)) {
		wac_compiler_scope_begin(state);
		wac_parser_statement_block(state);
		wac_compiler_scope_end(state);
	} else if (wac_parser_match(state, WAC_TOKEN_IF)) {
		wac_parser_statement_if(state);
	} else if (wac_parser_match(state, WAC_TOKEN_RETURN)) {
		wac_parser_statement_ret(state);
	} else if (wac_parser_match(state, WAC_TOKEN_WHILE)) {
		wac_parser_statement_while(state);
	} else if (wac_parser_match(state, WAC_TOKEN_FOR)) {
		wac_parser_statement_for(state);
	} else {
		wac_parser_statement_expr(state);
	}
}

static void wac_parser_sync(wac_state_t *state) {
	state->parser.panic = false;

	while (state->parser.curr.type != WAC_TOKEN_EOF) {
		if (state->parser.prev.type == WAC_TOKEN_SEMICOLON) return;
		switch (state->parser.curr.type) {
			case WAC_TOKEN_VAR:
			case WAC_TOKEN_CLASS:
			case WAC_TOKEN_FUN:
			case WAC_TOKEN_RETURN:
			case WAC_TOKEN_IF:
			case WAC_TOKEN_FOR:
			case WAC_TOKEN_WHILE:
				return;
			default:
				;;
		}
		wac_parser_advance(state);
	}
}

static uint32_t wac_parser_const_id(wac_state_t *state, wac_token_t *name) {
	return wac_page_addConst(state, &state->compiler->fun->page, WAC_VAL_OBJ(wac_obj_string_copy(state, name->start, name->len)));
}

static void wac_compiler_local_add(wac_state_t *state, wac_token_t name) {
	if (state->compiler->locals_asize <= state->compiler->locals_usize) {
		size_t oldSize = state->compiler->locals_asize;
		state->compiler->locals_asize *= WAC_ARRAY_GROW_MUL;
		state->compiler->locals = WAC_ARRAY_GROW(state, wac_local_t, state->compiler->locals, oldSize, state->compiler->locals_asize);
	}
	wac_local_t *local = &state->compiler->locals[state->compiler->locals_usize++];
	local->name = name;
	local->depth = INVALID_UINT;
	local->isCaptured = false;
}

static bool wac_parser_idEqual(wac_token_t *a, wac_token_t *b) {
	if (a->len != b->len) return false;
	return !memcmp(a->start, b->start, a->len);
}

static uint32_t wac_compiler_resolve_local(wac_parser_t *parser, wac_compiler_t *compiler, wac_token_t *name) {
	size_t i;
	wac_local_t *local;
	for (i = compiler->locals_usize - 1; i != INVALID_SIZE; --i) {
		local = &compiler->locals[i];

		if (wac_parser_idEqual(name, &local->name)) {
			if (local->depth == INVALID_UINT32) {
				wac_parser_error(parser, "Can't read local variable in its own initializer");
			}
			return (uint32_t)i;
		}
	}

	return INVALID_UINT32;
}

static uint32_t wac_compiler_upval_add(wac_state_t *state, wac_compiler_t *compiler, uint32_t index, bool isLocal) {
	size_t i, upvals_usize = compiler->fun->upvals_usize;
	wac_upval_t *upval;

	for (i = 0; i < upvals_usize; i++) {
		upval = &compiler->upvals[i];
		if (upval->index == index && upval->isLocal == isLocal)
			return (uint32_t)i;
	}

	if (compiler->upvals_asize <= upvals_usize) {
		size_t oldSize = compiler->upvals_asize;
		compiler->upvals_asize *= WAC_ARRAY_GROW_MUL;
		compiler->upvals = WAC_ARRAY_GROW(state, wac_upval_t, compiler->upvals, oldSize, compiler->upvals_asize);
	}

	compiler->upvals[upvals_usize].isLocal = isLocal;
	compiler->upvals[upvals_usize].index = index;
	return (uint32_t)compiler->fun->upvals_usize++;
}

static uint32_t wac_compiler_resolve_upval(wac_state_t *state, wac_compiler_t *compiler, wac_token_t *name) {
	if (!compiler->prev) return INVALID_UINT32;

	uint32_t local = wac_compiler_resolve_local(&state->parser, compiler->prev, name);
	if (local != INVALID_UINT32) {
		compiler->prev->locals[local].isCaptured = true;
		return wac_compiler_upval_add(state, compiler, local, true);
	}

	uint32_t upval = wac_compiler_resolve_upval(state, compiler->prev, name);
	if (upval != INVALID_UINT32) {
		return wac_compiler_upval_add(state, compiler, upval, false);
	}

	return INVALID_UINT32;
}

static void wac_parser_decl_local(wac_state_t *state) {
	//if global -> return
	if (state->compiler->scopeDepth == 0) return;

	wac_token_t *name = &state->parser.prev;

	size_t i;
	wac_local_t *local;
	for (i = state->compiler->locals_usize - 1; i != INVALID_SIZE; --i) {
		local = &state->compiler->locals[i];

		if (local->depth != INVALID_UINT && local->depth < state->compiler->scopeDepth) break;

		if (wac_parser_idEqual(name, &local->name)) wac_parser_error(&state->parser, "Already a variable with this name in this scope");
	}

	wac_compiler_local_add(state, *name);
}

static uint32_t wac_parser_var_parse(wac_state_t *state, const char *msg) {
	wac_parser_eat(state, WAC_TOKEN_ID, msg);

	//handle local
	wac_parser_decl_local(state);
	if (state->compiler->scopeDepth > 0) return 0;

	return wac_parser_const_id(state, &state->parser.prev);
}

static void wac_compiler_local_mark(wac_compiler_t *compiler) {
	if (compiler->scopeDepth == 0) return;
	compiler->locals[compiler->locals_usize - 1].depth = compiler->scopeDepth;
}

static void wac_parser_var_define(wac_state_t *state, uint32_t var) {
	//if local -> return
	if (state->compiler->scopeDepth > 0) {
		wac_compiler_local_mark(state->compiler);
		return;
	}
	wac_compiler_emit_5bytes(state, WAC_OP_DEFINE_GLOBAL, var);
}

static void wac_parser_decl_var(wac_state_t *state) {
	uint32_t var = wac_parser_var_parse(state, "Expected variable name");

	if (wac_parser_match(state, WAC_TOKEN_EQUAL)) {
		wac_parser_expr(state);
	} else {
		wac_compiler_emit_byte(state, WAC_OP_NULL);
	}

	wac_parser_eat(state, WAC_TOKEN_SEMICOLON, "Expected ';' after variable declaration");

	wac_parser_var_define(state, var);
}

static void wac_parser_function(wac_state_t *state, wac_fun_type_t type) {
	wac_compiler_t compiler;
	wac_obj_fun_t *fun;
	size_t i;

	wac_compiler_init(state, &compiler, type);
	wac_compiler_scope_begin(state);

	wac_parser_eat(state, WAC_TOKEN_LPAREN, "Expected '(' after function name");

	if (!wac_parser_check(&state->parser, WAC_TOKEN_RPAREN)) {
		do {
			++state->compiler->fun->arity;
			wac_parser_var_define(state, wac_parser_var_parse(state, "Expected parameter name"));
		} while (wac_parser_match(state, WAC_TOKEN_COMMA));
	}

	wac_parser_eat(state, WAC_TOKEN_RPAREN, "Expected ')' after parameters");
	wac_parser_eat(state, WAC_TOKEN_LCURLY, "Expected '{' before function body");
	wac_parser_statement_block(state);

	fun = wac_compiler_end(state);
	wac_compiler_emit_const(state, WAC_OP_CLOSURE, WAC_VAL_OBJ(fun));

	for (i = 0; i < fun->upvals_usize; ++i) {
		wac_compiler_emit_5bytes(state, compiler.upvals[i].isLocal ? 1 : 0, compiler.upvals[i].index);
	}

	WAC_ARRAY_FREE(state, wac_upval_t, compiler.upvals, compiler.upvals_asize);
}


static void wac_parser_decl_fun(wac_state_t *state) {
	uint32_t var = wac_parser_var_parse(state, "Expected function name");
	wac_compiler_local_mark(state->compiler);
	wac_parser_function(state, WAC_FUN_TYPE_FUN);
	wac_parser_var_define(state, var);
}

static void wac_parser_decl(wac_state_t *state) {
	if (wac_parser_match(state, WAC_TOKEN_VAR)) {
		wac_parser_decl_var(state);
	} else if (wac_parser_match(state, WAC_TOKEN_FUN)) {
		wac_parser_decl_fun(state);
	} else {
		wac_parser_statement(state);
	}

	if (state->parser.panic) wac_parser_sync(state);
}

static void wac_parser_var_named(wac_state_t *state, bool canAssign, wac_token_t name) {
	uint8_t get, set;
	uint32_t arg = wac_compiler_resolve_local(&state->parser, state->compiler, &name);

	if (arg != INVALID_UINT32) {
		//local
		get = WAC_OP_GET_LOCAL;
		set = WAC_OP_SET_LOCAL;
	} else if ((arg = wac_compiler_resolve_upval(state, state->compiler, &name)) != INVALID_UINT32) {
		get = WAC_OP_GET_UPVAL;
		set = WAC_OP_SET_UPVAL;
	} else {
		//global
		arg = wac_parser_const_id(state, &name);
		get = WAC_OP_GET_GLOBAL;
		set = WAC_OP_SET_GLOBAL;
	}

	if (canAssign && wac_parser_match(state, WAC_TOKEN_EQUAL)) {
		wac_parser_expr(state);
		wac_compiler_emit_5bytes(state, set, arg);
	} else {
		wac_compiler_emit_5bytes(state, get, arg);
	}
}

static void wac_parser_variable(wac_state_t *state, bool canAssign) {
	wac_parser_var_named(state, canAssign, state->parser.prev);
}

static void wac_parser_and(wac_state_t *state, bool canAssign) {
	size_t jmpAddr = wac_compiler_emit_jmp_forw(state, WAC_OP_JMP_FALSE);

	wac_compiler_emit_byte(state, WAC_OP_POP);
	wac_parser_prec(state, WAC_PREC_AND);
	wac_compiler_patchJmp(&state->compiler->fun->page, jmpAddr);
}

static void wac_parser_or(wac_state_t *state, bool canAssign) {
	size_t jmpAddr = wac_compiler_emit_jmp_forw(state, WAC_OP_JMP_TRUE);

	wac_compiler_emit_byte(state, WAC_OP_POP);
	wac_parser_prec(state, WAC_PREC_OR);
	wac_compiler_patchJmp(&state->compiler->fun->page, jmpAddr);
}

static uint32_t wac_parser_argc(wac_state_t *state) {
	uint32_t argc = 0;

	if (!wac_parser_check(&state->parser, WAC_TOKEN_RPAREN)) {
		do {
			wac_parser_expr(state);
			++argc;
		} while (wac_parser_match(state, WAC_TOKEN_COMMA));
	}

	wac_parser_eat(state, WAC_TOKEN_RPAREN, "Expected ')' after arguments");
	return argc;
}

static void wac_parser_call(wac_state_t *state, bool canAssign) {
	wac_compiler_emit_5bytes(state, WAC_OP_CALL, wac_parser_argc(state));
}

wac_obj_fun_t* wac_compiler_compile(wac_state_t *state, const char *src) {
	wac_scanner_init(&state->scanner, src);
	wac_compiler_t compiler;
	wac_compiler_init(state, &compiler, WAC_FUN_TYPE_SCRIPT);

	state->parser.error = false;
	state->parser.panic = false;

	wac_parser_advance(state);

	//wac_parser_expr(state);
	//wac_parser_eat(state, WAC_TOKEN_EOF, "Expected end of expression");

	while (!wac_parser_match(state, WAC_TOKEN_EOF)) {
		wac_parser_decl(state);
	}

	wac_obj_fun_t *fun = wac_compiler_end(state);
	return state->parser.error ? NULL : fun;
}
