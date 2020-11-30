#include <stdio.h>
#include <stdlib.h>			// to display errors

#include "common.h"
#include "compiler.h"
#include "scanner.h"

/*	A compiler has two jobs really:
	- it parses the user's source code
	- it takes knowledge and outputs low-level instructions that produce the same semantics

	this is a SINGLE_PASS COMPILER -> both processes above are 'threaded' together, unlike an AST
IMPORTANT:
	->this compiler COMPILES an EXPRESSION only, not the whole code, hence it is more of an INTERPRETER
*/


// to store current and previous tokens
typedef struct
{
	Token current;
	Token previous;
	bool hadError;		// flag to tell whether the code has a syntax error or no
	bool panicMode;		// flag for error cascades/multiple errors so the parser does not get confused, only returns the first
} Parser;

Parser parser;

Chunk* compilingChunk;		// declare Chunk pointer

static Chunk* currentChunk()
{
	return compilingChunk;
}

// to handle syntax errors
static void errorAt(Token* token, const char* message)
{
	if (parser.panicMode) return;		// if an error already exists
	parser.panicMode = true;

	fprintf(stderr, "Error at [Line %d]", token->line);

	if (token->type == TOKEN_EOF)
	{
		fprintf(stderr, " at end");
	}
	else if (token->type == TOKEN_ERROR)
	{
		// nothing
	}
	else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}


static void errorAtCurrent(const char* message)
{
	errorAt(&parser.current, message);
}

static void advance()
{
	parser.previous = parser.current;

	for (;;)
	{
		parser.current = scanToken();		// gets next token, stores it for later use
		if (parser.current.type != TOKEN_ERROR) break;			// if error is not found break

		errorAtCurrent(parser.current.start);			// start is the location/pointer of the token source code
	}
}


// a consume function, basically SKIPS to the next token
static void consume(TokenType type, const char* message)
{
	if (parser.current.type == type)			// if current token is equal to the token type being compared to
	{
		advance();
		return;
	}

	errorAtCurrent(message);
}


/* emitting BYTECODE for the VM to understand */
// the writeChunk for the compiler
static void emitByte(uint8_t byte)
{
	writeChunk(currentChunk(), byte, parser.previous.line);		// sends previous line so runtime errosr are associated with that line
}

// write chunk for multiple chunks, used to write an opcode followed by an operand(eg. in constants)
static void emitBytes(uint8_t byte1, uint8_t byte2)
{
	emitByte(byte1);
	emitByte(byte2);
}

static void emitReturn()
{
	emitByte(OP_RETURN);		// emit return type at the end of a compiler
}


static void endCompiler()
{
	emitReturn();
}

bool compile(const char* source, Chunk* chunk)
{
	initScanner(source);			// start scan/lexing
	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	advance();						// call to advance once to 'pump' the scanner
	//expression();
	consume(TOKEN_EOF, "Expect end of expression.");
	
	endCompiler();					// ends the expression with a return type
	return !parser.hadError;		// if no error return true
}

