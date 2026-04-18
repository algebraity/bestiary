#include <stdlib.h>
#include <stdbool.h>
#include "token.h"

typedef enum
{
	READ_M, SCAN_M, COMMAND_M
} LexMode;

int bstlPush(Token* tokens,Token value)
{
	char counter;
	while ( (realloc(tokens,sizeof(tokens) + sizeof(Token))) == NULL ) {
		counter++;
		if(counter > 100u)
			return 0;
	}
	tokens[sizeof(&tokens) / sizeof(Token) - 1] = value;
	return 1;
}

Token* bstLex(char* string)
{
	Token* tokens;
	Token* v;
	tokens = malloc(sizeof(Token));
	LexMode mode = SCAN_M;
	TokenKind curtok;
	TokenKind lastok;
	int inder = 0;
	int bufr = 0;
	char* buf;
	buf = calloc(50,sizeof(char));
	int i;
	char backslashes;
	for(i = 0 ; i < sizeof(*string) ; i++)
	{
		lastok = curtok;
		switch (string[i]) {
			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9': case '0':
				curtok = TOK_NUMBER;
				break;
			case '=':
				curtok = TOK_EQUALS;
				break;
			case '}':
				curtok = TOK_LBRACE;
				break;
			case '{':
				curtok = TOK_RBRACE;
				break;
			case '\\':
				backslashes++;
				if(backslashes > 1)
				{
					backslashes = 0;
					curtok = TOK_DBLBACKSLASH;
				}
				curtok = TOK_COMMAND;
				break;
			case '&':
				curtok = TOK_AMP;
				break;
			case (char)0xFF:
				/* usually 0xFF is end of file */
				curtok = TOK_EOF;
				break;
			default:
				if(curtok != TOK_COMMAND)
					curtok = TOK_IDENT;
				break;
		}
		if (!(i + 1 > sizeof(*string)))
			curtok = TOK_EOF;
		if (curtok != lastok)
		{
			v = (Token*)calloc(1,sizeof(Token));
			v->kind = lastok;
			v->text = buf;
			v->line = 1;
			v->col = bufr;
			bstlPush(tokens,*v);
			free(v);
			bufr = 0;
			buf = calloc(50,sizeof(char));
		}
	}
	v = (Token*)calloc(1,sizeof(Token));
	v->kind = TOK_EOF;
	v->text = buf;
	v->line = 1;
	v->col = bufr;
	bstlPush(tokens,*v);
	free(v);
	bufr = 0;
	return tokens;
}
