#include <stdio.h>
#include <string.h>

#include "wac_common.h"
#include "wac_scanner.h"

void wac_scanner_init(wac_scanner_t *scanner, const char *src) {
	scanner->start = src;
	scanner->curr = src;
	scanner->line = 1;
}

static bool wac_scanner_isAtEnd(wac_scanner_t *scanner) {
	return *scanner->curr == '\0';
}

static char wac_scanner_advance(wac_scanner_t *scanner) {
	return *scanner->curr++;
}

static bool wac_scanner_match(wac_scanner_t *scanner, char expected) {
	if (wac_scanner_isAtEnd(scanner)) return false;
	if (*scanner->curr != expected) return false;
	++scanner->curr;
	return true;
}

static void wac_scanner_skipWhitespace(wac_scanner_t *scanner) {
	for (;;) {
		switch (*scanner->curr) {
			case ' ':
			case '\r':
			case '\t':
				wac_scanner_advance(scanner);
				break;
			case '\n':
				++scanner->line;
				wac_scanner_advance(scanner);
				break;
			case '/':
				if (scanner->curr[1] == '/') {
					while (*scanner->curr != '\n' && !wac_scanner_isAtEnd(scanner)) wac_scanner_advance(scanner);
				} else {
					return;
				}
				break;
			default:
				return;
		}
	}
}

static bool wac_scanner_isDigit(char c) {
	return c >= '0' && c <= '9';
}

static bool wac_scanner_isAlpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static wac_token_t wac_scanner_token_make(wac_scanner_t *scanner, wac_token_type_t type) {
	wac_token_t token;
	token.type = type;
	token.start = scanner->start;
	token.len = scanner->curr - scanner->start;
	token.line = scanner->line;
	return token;
}

static wac_token_t wac_scanner_token_error(wac_scanner_t *scanner, const char *msg) {
	wac_token_t token;
	token.type = WAC_TOKEN_ERROR;
	token.start = msg;
	token.len = strlen(msg);
	token.line = scanner->line;
	return token;
}

static wac_token_type_t wac_scanner_checkKeyword(wac_scanner_t *scanner, size_t start, size_t len, const char *rest, wac_token_type_t type) {
	if (scanner->curr - scanner->start == start + len && memcmp(scanner->start + start, rest, len) == 0) return type;
	return WAC_TOKEN_ID;
}

static wac_token_type_t wac_scanner_id_type(wac_scanner_t *scanner) {
	switch (*scanner->start) {
		case 'v': return wac_scanner_checkKeyword(scanner, 1, 2, "ar", WAC_TOKEN_VAR);
		case 'c': return wac_scanner_checkKeyword(scanner, 1, 4, "lass", WAC_TOKEN_CLASS);
		case 't':
			if (scanner->curr - scanner->start > 1) {
				switch (scanner->start[1]) {
					case 'h': return wac_scanner_checkKeyword(scanner, 2, 2, "is", WAC_TOKEN_THIS);
					case 'r': return wac_scanner_checkKeyword(scanner, 2, 2, "ue", WAC_TOKEN_TRUE);
				}
			}
			break;
		case 's': return wac_scanner_checkKeyword(scanner, 1, 4, "uper", WAC_TOKEN_SUPER);
		case 'f':
			if (scanner->curr - scanner->start > 1) {
				switch (scanner->start[1]) {
					case 'u': return wac_scanner_checkKeyword(scanner, 2, 1, "n", WAC_TOKEN_FUN);
					case 'a': return wac_scanner_checkKeyword(scanner, 2, 3, "lse", WAC_TOKEN_FALSE);
					case 'o': return wac_scanner_checkKeyword(scanner, 2, 1, "r", WAC_TOKEN_FOR);
				}
			}
			break;
		case 'r': return wac_scanner_checkKeyword(scanner, 1, 5, "eturn", WAC_TOKEN_RETURN);
		case 'i': return wac_scanner_checkKeyword(scanner, 1, 1, "f", WAC_TOKEN_IF);
		case 'e': return wac_scanner_checkKeyword(scanner, 1, 3, "lse", WAC_TOKEN_ELSE);
		case 'w': return wac_scanner_checkKeyword(scanner, 1, 4, "hile", WAC_TOKEN_WHILE);
		case 'n': return wac_scanner_checkKeyword(scanner, 1, 3, "ull", WAC_TOKEN_NULL);
	}
	return WAC_TOKEN_ID;
}

static wac_token_t wac_scanner_id(wac_scanner_t *scanner) {
	while (wac_scanner_isAlpha(*scanner->curr) || wac_scanner_isDigit(*scanner->curr)) wac_scanner_advance(scanner);
	return wac_scanner_token_make(scanner, wac_scanner_id_type(scanner));
}

static wac_token_t wac_scanner_string(wac_scanner_t *scanner) {
	while (*scanner->curr != '"' && !wac_scanner_isAtEnd(scanner)) {
		if (*scanner->curr == '\n') ++scanner->line;
		wac_scanner_advance(scanner);
	}

	if (wac_scanner_isAtEnd(scanner)) return wac_scanner_token_error(scanner, "Unterminated string");

	wac_scanner_advance(scanner);
	return wac_scanner_token_make(scanner, WAC_TOKEN_STRING);
}

static wac_token_t wac_scanner_number(wac_scanner_t *scanner) {
	while (wac_scanner_isDigit(*scanner->curr)) wac_scanner_advance(scanner);

	if (*scanner->curr == '.' && wac_scanner_isDigit(scanner->curr[1])) {
		wac_scanner_advance(scanner);
		while (wac_scanner_isDigit(*scanner->curr)) wac_scanner_advance(scanner);
	}

	return wac_scanner_token_make(scanner, WAC_TOKEN_NUMBER);
}

wac_token_t wac_scanner_token_next(wac_scanner_t *scanner) {
	wac_scanner_skipWhitespace(scanner);
	scanner->start = scanner->curr;

	if (wac_scanner_isAtEnd(scanner)) return wac_scanner_token_make(scanner, WAC_TOKEN_EOF);

	char c = wac_scanner_advance(scanner);

	if (wac_scanner_isAlpha(c)) return wac_scanner_id(scanner);
	if (wac_scanner_isDigit(c)) return wac_scanner_number(scanner);

	switch (c) {
		case '(': return wac_scanner_token_make(scanner, WAC_TOKEN_LPAREN);
		case ')': return wac_scanner_token_make(scanner, WAC_TOKEN_RPAREN);
		case '{': return wac_scanner_token_make(scanner, WAC_TOKEN_LCURLY);
		case '}': return wac_scanner_token_make(scanner, WAC_TOKEN_RCURLY);
		case ',': return wac_scanner_token_make(scanner, WAC_TOKEN_COMMA);
		case '.': return wac_scanner_token_make(scanner, WAC_TOKEN_DOT);
		case '+': return wac_scanner_token_make(scanner, WAC_TOKEN_PLUS);
		case '-': return wac_scanner_token_make(scanner, WAC_TOKEN_MINUS);
		case '*': return wac_scanner_token_make(scanner, WAC_TOKEN_STAR);
		case '/': return wac_scanner_token_make(scanner, WAC_TOKEN_SLASH);
		case ';': return wac_scanner_token_make(scanner, WAC_TOKEN_SEMICOLON);

		case '!': return wac_scanner_token_make(scanner, wac_scanner_match(scanner, '=') ? WAC_TOKEN_BANG_EQUAL : WAC_TOKEN_BANG);
		case '=': return wac_scanner_token_make(scanner, wac_scanner_match(scanner, '=') ? WAC_TOKEN_EQUAL_EQUAL : WAC_TOKEN_EQUAL);
		case '>': return wac_scanner_token_make(scanner, wac_scanner_match(scanner, '=') ? WAC_TOKEN_GREATER_EQUAL : WAC_TOKEN_GREATER);
		case '<': return wac_scanner_token_make(scanner, wac_scanner_match(scanner, '=') ? WAC_TOKEN_LESS_EQUAL : WAC_TOKEN_LESS);

		case '&': if (!wac_scanner_isAtEnd(scanner) && wac_scanner_advance(scanner) == '&') return wac_scanner_token_make(scanner, WAC_TOKEN_AMPER_AMPER);
		case '|': if (!wac_scanner_isAtEnd(scanner) && wac_scanner_advance(scanner) == '|') return wac_scanner_token_make(scanner, WAC_TOKEN_BAR_BAR);

		case '"': return wac_scanner_string(scanner);

	}

	return wac_scanner_token_error(scanner, "Unexpected character");
}
