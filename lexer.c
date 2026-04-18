#include <stdlib.h>
#include <stdbool.h>

int bstlPush(void* tokens,char* value)
{
	char counter;
	while ( (realloc(tokens,sizeof(tokens) + sizeof(char*))) == NULL ) {
		counter++
		if(counter > 100u)
			return 0;
	}
	tokens[sizeof(tokens) / sizeof(char*) - 1] = value;
	return 1;
}

#define push bstlPush;

char** bstLex(char* string)
{
	char** tokens;
	/* TODO: start lexer here */
	return tokens;
}
