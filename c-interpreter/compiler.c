#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

void compile(const char* source)
{
	initScanner(source);			// scan/lexing

	// like an INTERPRETER, instructions are given one at a time 
	// scan a token until the compiler needs one

	int line = -1;
	for (;;)		// loops until the end of file token is reached
	{
		Token token = scanToken();
		if (token.line != line)
		{
			printf("%4d ", token.line);
		}
		else
		{
			printf("		|");
		}
		printf("%2d '%.*s'\n", token.type, token.length, token.start);

		if (token.type == TOKEN_EOF) break;
	}
}