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


#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// to store current and previous tokens
typedef struct
{
	Token current;
	Token previous;
	bool hadError;		// flag to tell whether the code has a syntax error or no
	bool panicMode;		// flag for error cascades/multiple errors so the parser does not get confused, only returns the first
} Parser;

// for precedence in unary operations
// ordered from lowest precedence to highest precedence
typedef enum
{
	PREC_NONE, 
	PREC_ASSIGNMENT,	// =
	PREC_OR,			// or
	PREC_AND,			// and
	PREC_EQUALITY,		// == !=
	PREC_COMPARISON,	// > >= < <=
	PREC_TERM,			// + -
	PREC_FACTOR,		// * /
	PREC_UNARY,			// ! -
	PREC_CALL,			// . ()
	PREC_PRIMARY
} Precedence;


// simple typdef function type with no arguments and returns nothing
// acts like a "virtual" function , a void function that cam be overidden; actually a void but override it with ParseFn
typedef void(*ParseFn)();


/*	parse rule, what is needed:
-> a function to compile a PREFIX expression starting with token of that type
-> a function to cimpile an INFIX expression whose left operand is followed by a token of that type
-> precedence of an infix expression with the tokenas an operator
*/
typedef struct
{
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;


Parser parser;

Chunk* compilingChunk;		// declare Chunk pointer


static Chunk* currentChunk()
{
	return compilingChunk;
}

// to handle syntax errors
static void errorAt(Token* token, const char* message)
{
	if (parser.panicMode) return;		// if an error already exists, no need to run other errors
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

// error from token most recently CONSUMED
static void error(const char* message)
{
	errorAt(&parser.previous, message);
}


// handling error from token, the most current one being handed, not yet consumed
static void errorAtCurrent(const char* message)			// manually provide the message
{
	errorAt(&parser.current, message);				// pass in the current parser
}


/* main compile functions */
// pump the compiler, basically go to / 'read' the next token, a SINGLE token
static void advance()
{
	parser.previous = parser.current;		//  store next parser as current

	for (;;)
	{
		parser.current = scanToken();		// gets next token, stores it for later use(the next scan) 
		if (parser.current.type != TOKEN_ERROR) break;			// if error is not found break

		errorAtCurrent(parser.current.start);			// start is the location/pointer of the token source code
	}
}


// SIMILAR to advance but there is a validation for a certain type
// syntax error comes from here, where it is known/expected what the next token will be
static void consume(TokenType type, const char* message)
{
	if (parser.current.type == type)			// if current token is equal to the token type being compared to
	{
		advance();
		return;
	}

	errorAtCurrent(message);		// if consumes a different type, error
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

//  emit specific return type
static void emitReturn()
{
	emitByte(OP_RETURN);		// emit return type at the end of a compiler
}

// to insert into constant table
static uint8_t makeConstant(Value value)
{
	int constant = addConstant(currentChunk(), value);
	if (constant > UINT8_MAX)
	{
		error("Too many constants in one chunk.");
		return 0;
	}

	return (uint8_t)constant;		// return as byte, the byte being the INDEX of the constantin the constats array
}

static void emitConstant(Value value)		// for constant emit the opcode, then the index
{
	emitBytes(OP_CONSTANT, makeConstant(value));	// add value to constant table
}

static void endCompiler()
{
	emitReturn();

	// for debugging
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError)
	{
		disassembleChunk(currentChunk(), "code");
	}
#endif
}



/* forwad declaration of main functions */
static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

/* expressions for different types of tokens */

// for binary, eg. 5 + 4
// or INFIX parser, where the operator is in the middle
// entire left hand expression has been compiled, and the infix operator has been consumed
// binary() handles the rest of the arithmetic operator
static void binary()
{
	// remember type of operator, already consumed
	TokenType  operatorType = parser.previous.type;
	
	// compile right operand
	ParseRule* rule = getRule(operatorType);		// the BIDMAS rule, operands in the right side have HIGHER PRECEDENCE
													// as binary operators are LEFT ASSOCIATIVE
	// recursively call parsePrecedence again
	parsePrecedence((Precedence)(rule->precedence + 1));		// conert from rule to enum(precedence) type

	switch (operatorType)
	{
		// note how NOT opcode is at the end
		// six binary operators for three instructions only(greater, not, equal)
	case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;		// add equal and not to the stack
	case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
	case TOKEN_GREATER: emitByte(OP_GREATER); break;
	case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
	case TOKEN_LESS: emitByte(OP_LESS);	break;
	case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;

	case TOKEN_PLUS:	emitByte(OP_ADD); break;
	case TOKEN_MINUS:	emitByte(OP_SUBTRACT); break;
	case TOKEN_STAR:	emitByte(OP_MULTIPLY); break;
	case TOKEN_SLASH:	emitByte(OP_DIVIDE); break;
	default:
		return;			// unreachable
	}
}

static void literal()
{
	switch (parser.previous.type)
	{
	case TOKEN_FALSE: emitByte(OP_FALSE); break;
	case TOKEN_TRUE: emitByte(OP_TRUE); break;
	case TOKEN_NULL: emitByte(OP_NULL); break;

	default:		// unreachable
		return;
	}
}

// parentheses for grouping
static void grouping()
{
	// assume initial ( has already been consumed, and recursively call to expression() to compile between the parentheses
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");	// expects a right parentheses, if not received then  error
}


/* parsing the tokens */
static void number()
{
	// strtod below converts from string to double
	// assume that token for the number literal has already been consumed and is stored in previous
	// double strtod(const char* str, char** endptr)
	// endptr is the first non-double character after teh char* str character; if none then null
	/*	The way it works:
	-> in scanner, if a digit exists after a digit, it advances() (skips) the current
	-> hence, we get that the start points to the START of the digit, and using strtod smartly it reaches until the last digit
	*/
	double value = strtod(parser.previous.start, NULL);		
	//printf("num %c\n", *parser.previous.start);
	emitConstant(NUMBER_VAL(value));
}

// unary
static void unary()
{
	TokenType operatorType = parser.previous.type;		// leading - token has already been consumed 

	// compile operand
	expression();

	switch (operatorType)
	{
	case TOKEN_BANG: emitByte(OP_NOT); break;


		// OP_NEGATE should be emitted last, AFTER the constant itself 
		// eg. say 4 - 5; 5 needs to be emitted and added to the chunk->constants first before OP_NEGATE
		/* it is important to take note of the precedence
		e.g -a.b + 3;
		when the unary negation is called, all of a.b + 3 will be consumed in expression(). Hence, a method is needed
		to STOP when + is found, or generally when an operand of LOWER PRECEDENCE is found
		*/
	case TOKEN_MINUS: emitByte(OP_NEGATE); break;			
	default:
		return;		
	}
}


/* the array of ParseRules 
uses C99 DESIGNATED INITIALIZER syntax
use {struct members} to initialize a struct
[index number] = {struct members}, the index number can be seen clearly
token enums from scanner is reused
*/
ParseRule rules[] =
{
	[TOKEN_LEFT_PAREN]		= {grouping,	NULL,	 PREC_NONE},	
	[TOKEN_RIGHT_PAREN]		= {NULL,     NULL,   PREC_NONE},
	[TOKEN_LEFT_BRACE]		= {NULL,     NULL,   PREC_NONE},
	[TOKEN_RIGHT_BRACE]		= {NULL,     NULL,   PREC_NONE},
	[TOKEN_COMMA]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_DOT]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_MINUS]			= {unary,    binary, PREC_TERM},
	[TOKEN_PLUS]			= {NULL,     binary, PREC_TERM},
	[TOKEN_SEMICOLON]		= {NULL,     NULL,   PREC_NONE},
	[TOKEN_SLASH]			= {NULL,     binary, PREC_FACTOR},
	[TOKEN_STAR]			= {NULL,     binary, PREC_FACTOR},
	[TOKEN_BANG]			= {unary,     NULL,   PREC_NONE},
	[TOKEN_BANG_EQUAL]		= {NULL,     binary,   PREC_EQUALITY},	// equality precedence
	[TOKEN_EQUAL]			= {NULL,     binary,   PREC_COMPARISON},		// comaprison precedence
	[TOKEN_EQUAL_EQUAL]		= {NULL,     binary,   PREC_COMPARISON},
	[TOKEN_GREATER]			= {NULL,     binary,   PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL]	= {NULL,     binary,   PREC_COMPARISON},
	[TOKEN_LESS]			= {NULL,     binary,   PREC_COMPARISON},
	[TOKEN_LESS_EQUAL]		= {NULL,      binary,  PREC_COMPARISON},
	[TOKEN_IDENTIFIER]		= {NULL,     NULL,   PREC_NONE},
	[TOKEN_STRING]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_NUMBER]			= {number,   NULL,   PREC_NONE},
	[TOKEN_AND]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_CLASS]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_ELSE]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_FALSE]			= {literal,     NULL,   PREC_NONE},
	[TOKEN_FOR]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_FUN]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_IF]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_NULL]			= {literal,     NULL,   PREC_NONE},
	[TOKEN_OR]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_PRINT]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_RETURN]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_SUPER]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_THIS]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_TRUE]			= {literal,     NULL,   PREC_NONE},
	[TOKEN_VAR]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_WHILE]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_ERROR]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_EOF]				= {NULL,     NULL,   PREC_NONE},
};




// for inserting where the unary operator should lie
// starts at current token and parses any expression at the given precedence level or higher
// for example, if parsePrecedence(PREC_COMPARISON) is called, it will parse unaries, terms, and factors
// ubt not or, and or assignment operators as they are lower. Basically parse anything that is ABOVE the given precedence
static void parsePrecedence(Precedence precedence)
{

	/*	PREFIX FIRST
	look up for a prefix token, and the FIRSt token is ALWAYS going to be a prefix
	*/
	advance();		// again, go next first then use previous type as the 'current' token
	// the way the compiler is designed is that it has to always have a prefix
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (prefixRule == NULL)
	{
		error("Expect expression.");
		return;
	}

	prefixRule();			// call the prefix function, may consume a lot of tokens

	/* after prefix expression is done, look for infix expression
	IMPORTANT: infix only runs if given precedence is LOWER than the operator for the infix
	*/
	while (precedence <= getRule(parser.current.type)->precedence)
	{
		advance();
		//printf("advanced");
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule();
	}
}

// get pointer to ParseRule struct according to type parameter
static ParseRule* getRule(TokenType type)
{
	return &rules[type];		
}

static void expression()
{
	parsePrecedence(PREC_ASSIGNMENT);	// as assignment is the 2nd lowest, parses evrything
}


bool compile(const char* source, Chunk* chunk)
{
	initScanner(source);			// start scan/lexing
	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	advance();						// call to advance once to 'pump' the scanner
	expression();
	consume(TOKEN_EOF, "Expect end of expression.");
	
	endCompiler();					// ends the expression with a return type
	return !parser.hadError;		// if no error return true
}

