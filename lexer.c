#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "token.h"

typedef enum
{
	SCAN_M, COMMAND_M
} LexMode;

// bstlPush now actually grows the backing buffer
int bstlPush(Token** tokens, size_t* len, size_t* cap, Token value)
{
	if (*len == *cap) {
		size_t newcap = *cap ? *cap * 2 : 8;
		Token* grown = realloc(*tokens, newcap * sizeof(Token));
		if (!grown) return 0;
		*tokens = grown;
		*cap = newcap;
	}
	(*tokens)[(*len)++] = value;
	return 1;
}

static void bstlFlush(Token** tokens, size_t* len, size_t* cap,
                      TokenKind kind, char* buf, size_t* bufr,
                      size_t line, size_t col)
{
	if (*bufr == 0) return;
	buf[*bufr] = '\0';
	char* text = malloc(*bufr + 1);
	memcpy(text, buf, *bufr + 1);
	Token t;
	t.kind = kind;
	t.text = text;
	t.line = line;
	t.col  = col;
	bstlPush(tokens, len, cap, t);
	*bufr = 0;
}

Token* bstLex(char* string, size_t* out_len)
{
	Token* tokens = NULL;
	size_t len = 0, cap = 0;
	LexMode mode = SCAN_M;          // Mode is used now, was not before
	TokenKind curtok = TOK_NONE;
	TokenKind lastok = TOK_NONE;
	size_t bufr = 0;
	size_t bufcap = 256;
	char* buf = calloc(bufcap, sizeof(char));
	size_t line = 1, col = 1, tokcol = 1;
	size_t l = strlen(string);

	for (size_t i = 0; i < l; i++) {
		char c = string[i];
		lastok = curtok;

		// Consume alphabet characters
		if (mode == COMMAND_M) {
			if (isalpha((unsigned char)c)) {
				if (bufr + 1 >= bufcap) {
					bufcap *= 2;
					buf = realloc(buf, bufcap);
				}
				buf[bufr++] = c;
				col++;
				continue;
			}
			bstlFlush(&tokens, &len, &cap, TOK_COMMAND, buf, &bufr, line, tokcol);
			mode = SCAN_M;
			curtok = TOK_NONE;
			lastok = TOK_NONE;
		}

		switch (c) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				// If we were mid-decimal (saw NUMBER then '.'), stay in DECIMAL.
				curtok = (lastok == TOK_DECIMAL) ? TOK_DECIMAL : TOK_NUMBER;
				break;
			case '=':
				curtok = TOK_EQUALS;
				break;
			case '(':
				curtok = TOK_LPAREN;
				break;
			case ')':
				curtok = TOK_RPAREN;
				break;
			case '{':
				curtok = TOK_LBRACE;
				break;
			case '+':
				curtok = TOK_PLUS;
				break;
			case '-':
				curtok = TOK_MINUS;
				break;
			case '*':
				curtok = TOK_STAR;
				break;
			case '/':
				curtok = TOK_SLASH;
				break;
			case ',':
				curtok = TOK_COMMA;
				break;
			case '^':
				curtok = TOK_CARET;
				break;
			case ':':
				curtok = TOK_COLON;
				break;
			case ';':
				curtok = TOK_SEMICOLON;
				break;
			case '|':
				curtok = TOK_PIPE;
				break;
			case '<':
				curtok = TOK_LESS;
				break;
			case '>':
				curtok = TOK_GREATER;
				break;
			case '\'':
				curtok = TOK_PRIME;
				break;
			case '$':
				curtok = TOK_DOLLAR;
				break;
			case '%':
				curtok = TOK_PERCENT;
				break;
			case '~':
				curtok = TOK_TILDE;
				break;
			case '#':
				curtok = TOK_HASH;
				break;
			case '_':
				curtok = TOK_UNDERSCORE;
				break;
			case '[':
				curtok = TOK_LBRACK;
				break;
			case ']':
				curtok = TOK_RBRACK;
				break;
			case '.':
				// Promote a number-in-progress to DECIMAL if the next char is a digit.
				// Otherwise '.' is its own TOK_DOT token.
				if (lastok == TOK_NUMBER && i + 1 < l && isdigit((unsigned char)string[i + 1])) {
					if (bufr + 1 >= bufcap) {
						bufcap *= 2;
						buf = realloc(buf, bufcap);
					}
					buf[bufr++] = '.';
					curtok = TOK_DECIMAL;
					col++;
					continue;
				}
				curtok = TOK_DOT;
				break;
			case '}':
				curtok = TOK_RBRACE;
				break;
			case '\\':
				// Detect \\ by one-char lookahead
				bstlFlush(&tokens, &len, &cap, curtok, buf, &bufr, line, tokcol);
				if (i + 1 < l && string[i + 1] == '\\') {
					Token t;
					t.kind = TOK_DBLBACKSLASH;
					t.text = NULL;
					t.line = line;
					t.col  = col;
					bstlPush(&tokens, &len, &cap, t);
					i++;
					col += 2;
					curtok = TOK_NONE;
				} else {
					mode = COMMAND_M;
					curtok = TOK_COMMAND;
					tokcol = col;
					col++;
				}
				continue;
			case '&':
				curtok = TOK_AMP;
				break;
			case ' ': case '\t':
				bstlFlush(&tokens, &len, &cap, curtok, buf, &bufr, line, tokcol);
				curtok = TOK_NONE;
				col++;
				continue;
			case '\n':
				bstlFlush(&tokens, &len, &cap, curtok, buf, &bufr, line, tokcol);
				curtok = TOK_NONE;
				line++;
				col = 1;
				continue;
			default:
				curtok = TOK_IDENT;
				break;
		}

		// Fixed inverted EOF loop :3
		// Flush when token type changes
		if (curtok != lastok)
			bstlFlush(&tokens, &len, &cap, lastok, buf, &bufr, line, tokcol);
		if (bufr == 0) tokcol = col;
		if (bufr + 1 >= bufcap) {
			bufcap *= 2;
			buf = realloc(buf, bufcap);
		}
		buf[bufr++] = c;     // store char in buf--was not done before, I think
		col++;

		// Only TOK_IDENT and TOK_NUMBER accumulate across characters;
		// every other kind is one char per token, so flush right away.
		// This prevents "((" from collapsing into a single LPAREN, etc.
		if (curtok != TOK_IDENT && curtok != TOK_NUMBER && curtok != TOK_DECIMAL) {
			bstlFlush(&tokens, &len, &cap, curtok, buf, &bufr, line, tokcol);
			curtok = TOK_NONE;
		}
	}

	/// Flush any trailing accumulated token.
	if (mode == COMMAND_M)
		bstlFlush(&tokens, &len, &cap, TOK_COMMAND, buf, &bufr, line, tokcol);
	else
		bstlFlush(&tokens, &len, &cap, curtok, buf, &bufr, line, tokcol);
	free(buf);

	// emit TOK_EOF once loop exits, only once
	Token eof;
	eof.kind = TOK_EOF;
	eof.text = NULL;
	eof.line = line;
	eof.col  = col;
	bstlPush(&tokens, &len, &cap, eof);

	if (out_len) *out_len = len;
	return tokens;
}
