#include <stdlib.h>

char* bstlPush(void* stack,char* value) {
	return realloc(stack,sizeof(stack) + sizeof(char*));
}

char* bstlPop(void* stack) {
	return realloc(stack,sizeof(stack) - sizeof(char*));
}

#define push bstlPush;
#define pop bstlPop;

char** bstLex(char* string)
{
	char** tree;
	/* TODO: start lexer here */
	return tree;
}
