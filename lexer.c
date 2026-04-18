#include <stdlib.h>
#include <stdbool.h>
#include "token.h"

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

#define push bstlPush;

Token* bstLex(char* string)
{
	Token* tokens;
	tokens = malloc(sizeof(Token));
	/* TODO: start lexer here */
	return tokens;
}
