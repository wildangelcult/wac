#ifndef __WAC_SCANNER_H
#define __WAC_SCANNER_H

#include "wac_common.h"

typedef struct wac_scanner_s {
	const char *start;
	const char *curr;
	size_t line;
} wac_scanner_t;

typedef enum wac_token_type_e {
	WAC_TOKEN_LPAREN,
	WAC_TOKEN_RPAREN,
	WAC_TOKEN_LCURLY,
	WAC_TOKEN_RCURLY,
	WAC_TOKEN_LSQUARE,
	WAC_TOKEN_RSQUARE,
	WAC_TOKEN_COMMA,
	WAC_TOKEN_DOT,
	WAC_TOKEN_PLUS,
	WAC_TOKEN_MINUS,
	WAC_TOKEN_STAR,
	WAC_TOKEN_SLASH,
	WAC_TOKEN_SEMICOLON, 

	WAC_TOKEN_BANG,
	WAC_TOKEN_BANG_EQUAL,
	WAC_TOKEN_EQUAL,
	WAC_TOKEN_EQUAL_EQUAL,
	WAC_TOKEN_GREATER,
	WAC_TOKEN_GREATER_EQUAL,
	WAC_TOKEN_LESS,
	WAC_TOKEN_LESS_EQUAL,

	WAC_TOKEN_AMPER_AMPER,
	WAC_TOKEN_BAR_BAR,

	WAC_TOKEN_ID,
	WAC_TOKEN_STRING,
	WAC_TOKEN_NUMBER,

	WAC_TOKEN_VAR,
	WAC_TOKEN_CLASS,
	WAC_TOKEN_THIS,
	WAC_TOKEN_SUPER,
	WAC_TOKEN_FUN,
	WAC_TOKEN_RETURN,
	WAC_TOKEN_IF,
	WAC_TOKEN_ELSE,
	WAC_TOKEN_TRUE,
	WAC_TOKEN_FALSE,
	WAC_TOKEN_FOR,
	WAC_TOKEN_WHILE,
	WAC_TOKEN_NULL,

	WAC_TOKEN_ERROR,
	WAC_TOKEN_EOF
} wac_token_type_t;

typedef struct wac_token_s {
	wac_token_type_t type;
	const char *start;
	size_t len;
	size_t line;
} wac_token_t;

void wac_scanner_init(wac_scanner_t *scanner, const char *src);
wac_token_t wac_scanner_token_next(wac_scanner_t *scanner);

#endif //__WAC_SCANNER_H
