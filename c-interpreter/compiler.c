#include <stdio.h>
#include <stdlib.h>			// to display errors
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "memory.h"			// for switch statements and marking the roots

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
typedef void(*ParseFn)(bool canAssign);


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

typedef struct
{
	Token name;
	int depth;			// depth of the variable, corresponding to scoreDepth in the struct below
	bool isCaptured;	// track whether the local is captured by a closure or no
} Local;


typedef struct
{
	bool isLocal;
	int index;			// matches the index of the local variable in ObjClosure
} Upvalue;	

typedef enum
{
	TYPE_FUNCTION,
	TYPE_SCRIPT		// top level main()
} FunctionType;

// stack for local variables
typedef struct Compiler		// give name to struct itself(the name comes after), only used below in enclosing
{
	struct Compiler* enclosing;			// pointer to the 'outer'/enclosing compiler, to return to after function

	// wrapping the whole program into one big main() function
	ObjFunction* function;
	FunctionType type;

	Local locals[UINT8_COUNT];		// array to store locals, ordered in the order of declarations
	int localCount;					// tracks amount of locals in a scope
	Upvalue upvalues[UINT8_COUNT];
	int scopeDepth;					// number of scopes/blocks surrounding the code
} Compiler;

Parser parser;

Compiler* current = NULL;

static Chunk* currentChunk()
{
	return &current->function->chunk;
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

static bool check(TokenType type)
{
	return parser.current.type == type;			// check if current matches given
}


static bool match(TokenType type)
{
	if (!check(type)) return false;
	advance();
	return true;
}

/* emitting BYTECODE for the VM to understand */
// the writeChunk for the compiler
static void emitByte(uint8_t byte)
{
	writeChunk(currentChunk(), byte, parser.previous.line);		// sends previous line so runtime errors are associated with that line
}

// write chunk for multiple chunks, used to write an opcode followed by an operand(eg. in constants)
static void emitBytes(uint8_t byte1, uint8_t byte2)
{
	emitByte(byte1);
	emitByte(byte2);
}

// for looping statements
static void emitLoop(int loopStart)
{
	emitByte(OP_LOOP);

	// int below jumps back, + 2 accounting the OP_LOOP and the instruction's own operand
	int offset = currentChunk()->count - loopStart + 2;			
	if (offset > UINT16_MAX) error("Loop body too large.");

	emitByte((offset >> 8) & 0xff);
	emitByte(offset & 0xff);
}


static int emitJump(uint8_t instruction)
{
	/* backpatching */
	emitByte(instruction);	// writes a placeholder operand for jump offset
	emitByte(0xff);			// hexadecimal number with value of 255
	emitByte(0xff);

	// basically, get the difference in bytes before the two 0xff is added
	return currentChunk()->count - 2;
}

//  emit specific return type
static void emitReturn()
{
	emitByte(OP_NULL);			// for functions that return nothing
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

static void patchJump(int offset)
{
	// - 2 to adjust for the jump offset itself
	int jump = currentChunk()->count - offset - 2;

	if (jump > UINT16_MAX)
	{
		error("Too much code to jump over.");
	}

	// the patchJump provides the VALUE or amount to JUMP
	currentChunk()->code[offset] = (jump >> 8) & 0xff;		// right shift by 8, then bitwise AND with 255(oxff is 111111)
	currentChunk()->code[offset + 1] = jump & 0xff;			// only AND
}

// initialize the compiler
static void initCompiler(Compiler* compiler, FunctionType type)
{
	compiler->enclosing = current;			// the 'outer' compiler
	compiler->function = NULL;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	compiler->function = newFunction();
	current = compiler;				// current is the global variable pointer for the Compiler struct, point to to the parameter
									// basically assign the global pointer 

	// for functions
	if (type != TYPE_SCRIPT)
	{
		current->function->name = copyString(parser.previous.start, parser.previous.length);		// function name handled here
	}

	// compiler implicitly claims slot zero for local variables
	Local* local = &current->locals[current->localCount++];
	local->isCaptured = false;
	local->depth = 0;
	local->name.start = "";
	local->name.length = 0;
}

static ObjFunction* endCompiler()
{
	emitReturn();
	ObjFunction* function = current->function;

	// for debugging
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError)
	{
		disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");	// if name is NULL then it is the Script type(main()
	}
#endif

	current = current->enclosing;	// return back to enclosing compiler after function
	return function;			// return to free
}

static void beginScope()
{
	current->scopeDepth++;
}

static void endScope()
{
	current->scopeDepth--;

	// remove variables out of scope
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth)
	{
		/* at the end of a block scope, when the compiler emits code to free the stack slot for the locals, 
		tell which one to hoist to the heap
		*/
		if (current->locals[current->localCount - 1].isCaptured)	// if it is captured/used
		{
			emitByte(OP_CLOSE_UPVALUE);	// op code to move the upvalue to the heap
		}
		else{
			emitByte(OP_POP);			// if not used anymore/capture simply pop the value off the stack
		}

		current->localCount--;
	}
}



/* forwad declaration of main functions */
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);


/* variable declarations */
static uint8_t identifierConstant(Token* name)
{
	return makeConstant(OBJ_VAL(copyString(name->start, name->length)));	// add to constant table
}

static bool identifiersEqual(Token* a, Token* b)
{
	if (a->length != b->length) return false;
	return memcmp(a->start, b->start, a->length) == 0;
}


static int resolveLocal(Compiler* compiler, Token* name)
{
	for (int i = compiler->localCount - 1; i >= 0; i--)		// walk through the local variables
	{
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name))			
		{	
			if (local->depth == -1)
			{
				error("Cannot read local variable in its own initializer.");
			}
			return i;			// found the var, return the index
		}
	}
	
	return -1;			// not found, name is global variable
}


// add upvalue
static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal)
{
	int upvalueCount = compiler->function->upvalueCount;	// get current upvalue count

	// check whether the upvalue has already been declared
	for (int i = 0; i < upvalueCount; i++)
	{
		Upvalue* upvalue = &compiler->upvalues[i];			// get pointer for each upvalue in the array
		if (upvalue->index == index && upvalue->isLocal == isLocal)
		{
			return i;				// if found, return the index of the upvalue in the upvalue array
		}
	}

	if (upvalueCount == UINT8_COUNT)
	{
		error("Too many closure variables");
		return 0;
	}

	// compiler keeps an array of upvalue structs to track closed-over identifiers
	// indexes in the array match the indexes of ObjClosure at runtime
	// insert to upvalues array
	compiler->upvalues[upvalueCount].isLocal = isLocal;		// insert bool status
	compiler->upvalues[upvalueCount].index = index;			// insert index
	return compiler->function->upvalueCount++;				// increase count and return
}


/*	for closures
- resolveUpvalue looks for a local variable declared in any of the surrounding functions
- if it finds one it returns the index for that upvalue variable, otherwise returns -1
*/
static int resolveUpvalue(Compiler* compiler, Token* name)
{
	if (compiler->enclosing == NULL) return -1;		// if in main()

	int local = resolveLocal(compiler->enclosing, name);	// looks for local value in enclosing function/compiler
	if (local != -1)
	{
		compiler->enclosing->locals[local].isCaptured = true;	// mark local is captured/used by and upvalue
		return addUpvalue(compiler, (uint8_t)local, true);		// create up value
	}

	// recursion to solve nested upvalues
	// recursive call right in the middle
	int upvalue = resolveUpvalue(compiler->enclosing, name);	// if the enclosing function is main() (NULL), it returns -1
	if (upvalue != -1)
	{
		return addUpvalue(compiler, (uint8_t)upvalue, true);
	}


	return -1;
}


static void addLocal(Token name)
{
	if (current->localCount == UINT8_COUNT)
	{
		error("Too many local variables in block.");
		return;
	}

	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;			// for cases where a variable name is redefined inside another scope, using the variable itself
	local->isCaptured = false;
}

static void declareVariable()	// for local variables
{
	// global vars are implicitly declared, and are late bound, not 'initialized' here but in the VM
	if (current->scopeDepth == 0) return;


	/* local variable declaration happens below */
	Token* name = &parser.previous;

	// to not allow two variable declarations to have the same name
	// loop only checks to a HIGHER SCOPE; another block overlaping/shadowing is allowed
	// work backwards
	for (int i = current->localCount - 1; i >= 0; i--)			
	{
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth)	// if reach beginning of array(highest scope)
		{
			break;
		}

		if (identifiersEqual(name, &local->name))
		{
			error("Variable with this name exists in scope.");
		}
	}

	addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage)
{
	consume(TOKEN_IDENTIFIER, errorMessage);		// requires next token to be an identifier

	declareVariable();
	if (current->scopeDepth > 0) return 0;			// if scopeDepth is not 0, then it is a local not global var
	// return a dummy index
	// at runtime, locals are not looked up by name so no need to insert them to a table


	return identifierConstant(&parser.previous);	// return index from the constant table	
}


static void markInitialized()
{
	if (current->scopeDepth == 0) return;				// if global return
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global)
{
	if (current->scopeDepth > 0)
	{
		markInitialized();
		return;
	}

	emitBytes(OP_DEFINE_GLOBAL, global);	// opcode for declaration and the constant itself
}


// for function arguments, returns number of arguments
// each argument expression generates code which leaves value on the stack in preparation for the call
static uint8_t argumentList()
{
	uint8_t argCount = 0;
	if (!check(TOKEN_RIGHT_PAREN))		// if ) has not been reached
	{
		do
		{
			expression();		// collect the arguments
			
			if (argCount == 255)			// cannot have more than 255 arguments as each operand is a single byte(uint8_t)
			{		
				error("Cannot have more than 255 arguments.");
			}

			argCount++;
		} while (match(TOKEN_COMMA));
	}

	consume(TOKEN_RIGHT_PAREN, "Expect ')' after argument list.");
	return argCount;
}

static void and_(bool canAssign)
{
	int endJump = emitJump(OP_JUMP_IF_FALSE);		// left hand side is already compiled,
													// and if it is false skip it and go to next

	emitByte(OP_POP);
	parsePrecedence(PREC_AND);


	patchJump(endJump);
}



// for binary, eg. 5 + 4
// or INFIX parser, where the operator is in the middle
// entire left hand expression has been compiled, and the infix operator has been consumed
// binary() handles the rest of the arithmetic operator
static void binary(bool canAssign)
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


// for function calls
static void call(bool canAssign)
{
	// again, assumes the function itself(its call name) has been placed on the codestream stack
	uint8_t argCount = argumentList();		// compile arguments using argumentList
	emitBytes(OP_CALL, argCount);			// write on the chunk
}


static void literal(bool canAssign)
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
static void grouping(bool canAssign)
{
	// assume initial ( has already been consumed, and recursively call to expression() to compile between the parentheses
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");	// expects a right parentheses, if not received then  error
}


/* parsing the tokens */
static void number(bool canAssign)
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

static void or_(bool canAssign)
{
	// jump if left hand side is true
	int elseJump = emitJump(OP_JUMP_IF_FALSE);		// if left is false jump directly to right hand
	int endJump = emitJump(OP_JUMP);				// if not skipped(as left is true) jump the right hand

	patchJump(elseJump);
	emitByte(OP_POP);

	parsePrecedence(PREC_OR);
	patchJump(endJump);
}

// 'initialize' the string here
static void string(bool canAssign)
{
	// in a string, eg. "hitagi", the quotation marks are trimmed
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

// declare/call variables
static void namedVariable(Token name, bool canAssign)
{
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);		// try find a local variable with a given name
	if (arg != -1)
	{
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else if ((arg = resolveUpvalue(current, &name)) != -1)		// for upvalues
	{
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	}
	else
	{
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	
	// test case to check whether it is a get(just the name) or a reassignment
	if (canAssign && match(TOKEN_EQUAL))		// if a = follows right after
	{
		expression();
		emitBytes(setOp, (uint8_t)arg);			// reassignment/set
	}
	else
	{
		emitBytes(getOp, (uint8_t)arg);			// as normal get
		// printf("gest");
	}

}


static void variable(bool canAssign)
{
	namedVariable(parser.previous, canAssign);
}

// unary
static void unary(bool canAssign)
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
	// function calls are like infixes, with high precedence on the left, ( in the middle for arguments, then ) at the end
	[TOKEN_LEFT_PAREN]		= {grouping,	call,	 PREC_CALL},		// call for functions	
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
	[TOKEN_IDENTIFIER]		= {variable,     NULL,   PREC_NONE},
	[TOKEN_STRING]			= {string,     NULL,   PREC_NONE},
	[TOKEN_NUMBER]			= {number,   NULL,   PREC_NONE},
	[TOKEN_AND]				= {NULL,     and_,   PREC_AND},
	[TOKEN_CLASS]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_ELSE]			= {NULL,     NULL,   PREC_NONE},
	[TOKEN_FALSE]			= {literal,     NULL,   PREC_NONE},
	[TOKEN_FOR]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_FUN]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_IF]				= {NULL,     NULL,   PREC_NONE},
	[TOKEN_NULL]			= {literal,     NULL,   PREC_NONE},
	[TOKEN_OR]				= {NULL,     or_,   PREC_OR},
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

	//

	bool canAssign = precedence <= PREC_ASSIGNMENT;			// for assignment precedence	
	prefixRule(canAssign);			// call the prefix function, may consume a lot of tokens

	
	/* after prefix expression is done, look for infix expression
	IMPORTANT: infix only runs if given precedence is LOWER than the operator for the infix
	or more nicely if NEXT/INFIX PRECEDENCE IS HIGHER THAN PREC ASSIGNMENT(parameter above_
	*/

	
	while (precedence <= getRule(parser.current.type)->precedence)
	{
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		
		infixRule(canAssign);
	}

	//consume(TOKEN_AND, "consume and failed");

	if (canAssign && match(TOKEN_EQUAL))		// if = is not consumed as part of the expression, nothing will , hence an error
	{
		error("Invalid Assignment target.");
	}
}

// get pointer to ParseRule struct according to type parameter
static ParseRule* getRule(TokenType type)
{
	return &rules[type];		
}

static void expression()		// a single 'statement' or line
{
	parsePrecedence(PREC_ASSIGNMENT);	// as assignment is the 2nd lowest, parses evrything
}

static void block()
{
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))	// parse until EOF or right brace is 'peeked'
	{
		declaration();		// compile rest of block, keeps on parsing until right brace or EOF is 'peeked'		
	}

	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");			
}


/* functions */
static void function(FunctionType type)
{
	// create separate Compiler for each function
	Compiler compiler;
	initCompiler(&compiler, type);		// set new compiler(function) as the current one
	beginScope();

	// compile parameters
	consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
	
	if (!check(TOKEN_RIGHT_PAREN))		// if end ) has not been reached
	{
		do {
			current->function->arity++;		// add number of parameters
			if (current->function->arity > 255)
			{
				errorAtCurrent("Cannot have more than 255 parameters.");
			}

			uint8_t paramConstant = parseVariable("Expect variable name.");		// get name
			defineVariable(paramConstant);			// scope handled here already
		} while (match(TOKEN_COMMA));
	}	

	consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameter list.");

	// body
	consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
	block();

	// create function object
	ObjFunction* function = endCompiler();				// ends the current compiler
	// compilers are treated like a stack; if current one is ended, like above, return to the previous one

	// emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
	emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

	/*	by the time the compiler reaches the end of a function declaration,
	every variable reference hass been resolved as either local, upvalue or global.
	each upvalue may return a local var or another upvalue

	-> for each upvalue there are two single-byte operands
	-> if first byte is one, then it captures a local variable in the enclosing function
	-> if first byte is 0, it captures the function's upvalues
	*/
	
	for (int i = 0; i < function->upvalueCount; i++)
	{
		emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
		emitByte(compiler.upvalues[i].index);				 // emit index
	}

}


static void classDeclaration()
{
	consume(TOKEN_IDENTIFIER, "Expect class name.");
	uint8_t nameConstant = identifierConstant(&parser.previous);		// add to constant table as a string, return its index
	declareVariable();						// declare that name variable

	emitBytes(OP_CLASS, nameConstant);			// takes opcode and takes the constant table index
	defineVariable(nameConstant);			// add it to the global hasht; we must DEFINE AFTER DECLARE to use it

	consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
	consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
}


static void funDeclaration()
{
	uint8_t global = parseVariable("Expect function name.");
	markInitialized();					// scoping
	function(TYPE_FUNCTION);	
	defineVariable(global);
}

static void varDeclaration()
{
	uint8_t global = parseVariable("Expect variable name.");

	if (match(TOKEN_EQUAL))
	{
		expression();
	}
	else
	{
		emitByte(OP_NULL);		// not initialized
	}
	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	defineVariable(global);			// create global variable here; if local, not added to table
}

static void expressionStatement()
{
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(OP_POP);
}


static void forStatement()
{
	beginScope();			// for possible variable declarations in clause
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
	
	// initializer clause
	if (match(TOKEN_SEMICOLON))
	{
		// no initializer
	}
	else if (match(TOKEN_VAR))
	{
		varDeclaration();			// for clause scope only
	}
	else 
	{
		expressionStatement();
	}


	int loopStart = currentChunk()->count;
	
	//  the condition clause
	/* CONDITION CLAUSE
	1. If false, pop the recently calculated expression and skip the loop
	2. if true, go to the body; see increment clause below
	*/
	int exitJump = -1;
	if (!match(TOKEN_SEMICOLON))
	{
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

		// jump out of loop if condition is false
		exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP);				// still need to figure this out, most likely just deleting 'temporary' constants in the scope
	}

	// the increment clause
	if (!match(TOKEN_RIGHT_PAREN))		// if there is something else before the terminating ')'
	{
		/*	INCEREMENT CLAUSE
		1. from the condition clause, first jump OVER the increment, to the body
		2. in the body, run the body
		3. jump BACK to the increment and run it
		4. from the increment jump BACK to the CONDITION clause, back to the cycle
		*/
		int bodyJump = emitJump(OP_JUMP);		// jump the increment clause
	
		int incrementStart = currentChunk()->count;		// starting index for increment
		expression();			// run the for expression
		emitByte(OP_POP);		// pop expression constant
		consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

		// running the loop
		emitLoop(loopStart);		// goes back to the start of the CONDITION clause of the for loop
		loopStart = incrementStart;
		patchJump(bodyJump);
	}

	statement();

	emitLoop(loopStart);

	// patch the jump in the loop body
	if (exitJump != -1)
	{
		patchJump(exitJump);
		emitByte(OP_POP);		// only pop when THERE EXISTS A CONDITION from the clause
	}

	endScope();
}

// if method
static void ifStatement()
{
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	expression();													// compile the expression statment inside; parsePrecedence()
	// after compiling expression above conditon value will be left at the top of the stack
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	// gives an operand on how much to offset the ip; how many bytes of code to skip
	// if falsey, simply adjusts the ip by that amount
	// offset to jump to next (potentially else or elf) statment
	// insert to opcode the then branch statment first, then get offset
	int thenJump = emitJump(OP_JUMP_IF_FALSE);	/* this gets distance */


	/* use BACKPATCHING
	- emit jump first with a placeholder offset, and get how far to jump
	
	
	*/
	
	statement();

	// below jump wil SURELY jump; this is skipped if the first emitJump is not false
	int elseJump = emitJump(OP_JUMP);			// need to jump at least 'twice' with an else statement
												// if the original statement is  true, then skip the the else statement

	emitByte(OP_POP);		// if then statment is run; pop the expression inside () after if
	patchJump(thenJump);	/* this actually jumps */

	emitByte(OP_POP);		// if else statment is run; pop the expression inside () after if
	if (match(TOKEN_ELSE)) statement();

	if (match(TOKEN_ELF))	// else if
	{
		// go to statement, then go back to IF
		ifStatement();
	}

	/* this actually jumps */
	// last jump that is executed IF FIRST STATEMENT IS TRUE
	patchJump(elseJump);			// for the second jump
}

static void switchStatement()
{
	//printf("\nSwitch read \n");

	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
	if (!check(TOKEN_IDENTIFIER))	// check next token
	{
		errorAtCurrent("Expect identifier after switch.");
	}

	// if no error, consume the identifier
	expression();

	consume(TOKEN_RIGHT_PAREN, "Expect ')' after switch variable.");
	consume(TOKEN_COLON, "Expect ':' after switch declaration.");
	consume(TOKEN_CASE, "Expect at lest 1 case after switch declaration.");

	/* to store  opcode offsets */
	uint8_t casesCount = -1;
	uint8_t capacity = 0;
	int* casesOffset = ALLOCATE(int, 8);			// 8 initial switch cases

	do		// while next token is a case, match also advances
	{
		// grow array if needed
		if (capacity < casesCount + 1)
		{
			int oldCapacity = capacity;
			capacity = GROW_CAPACITY(oldCapacity);
			casesOffset = GROW_ARRAY(int, casesOffset, oldCapacity, capacity);
		}
		
		casesCount++; 

		expression();
		consume(TOKEN_COLON, "Expect ':' after case expression.");
		emitByte(OP_SWITCH_EQUAL);			// check if both values are equal

		int caseFalseJump = emitJump(OP_JUMP_IF_FALSE);					// jump if false
		//printf("\ncase false jump offset: %d", caseFalseJump);

		// parse the statment
		statement();

		emitByte(OP_POP);		// pop the 'true' from OP_SWITCH_EQUAL
		casesOffset[casesCount] = emitJump(OP_JUMP);
		//printf("\ncase true jump offset: %d", casesOffset[casesCount]);
		
		// jump to end of case if false
		patchJump(caseFalseJump);
		emitByte(OP_POP);		// pop the 'false' statment from OP_SWITCH_EQUAL
	} while (match(TOKEN_CASE));

	if (match(TOKEN_DEFAULT))
	{
		consume(TOKEN_COLON, "Expect ':' default case.");
		statement();		// running the default statement
	}
	//consume(TOKEN_DEFAULT, "Default case not provided for switch.");


	// patchJump for each available jump
	for (uint8_t i = 0; i <= casesCount; i++)
	{
		patchJump(casesOffset[i]);
	}
		
	emitByte(OP_POP);			// pop switch constant
	FREE_ARRAY(int, casesOffset, capacity);
}


static void printStatement()
{
	expression();			// this is the function that actually processes the experssion
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");		// try consume ;, if fails show message
	emitByte(OP_PRINT);
}

static void returnStatement()
{
	if (current->type == TYPE_SCRIPT)
	{
		error("Cannot return from top-level code.");
	}
	if (match(TOKEN_SEMICOLON))
	{
		emitReturn();
	}
	else
	{
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
		emitByte(OP_RETURN);
	}
}

static void whileStatement()
{
	int loopStart = currentChunk()->count;		// index where the statement to loop starts

	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int exitJump = emitJump(OP_JUMP_IF_FALSE);			// skip stament if condition is false

	emitByte(OP_POP);			// pop the last expression(true or false)
	statement();

	emitLoop(loopStart);		// method to 'loop' the instruction

	patchJump(exitJump);
	emitByte(OP_POP);
}

static void synchronize()
{
	parser.panicMode = false;

	//printf("panic mode");

	// basically turn off the 'error' mode and skips token until something that looks like a statement boundary is found
	// skips tokens indiscriminately until somehing that looks like a statement boundary(eg. semicolon) is found
	while (parser.current.type != TOKEN_EOF)
	{
		if (parser.previous.type == TOKEN_SEMICOLON) return;

		switch (parser.current.type)
		{
		case TOKEN_CLASS:
		case TOKEN_FUN:
		case TOKEN_VAR:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_PRINT:
		case TOKEN_RETURN:
			return;
		default: // do nothing 
			;
		}

		advance();
	}
}

static void declaration()
{
	if (match(TOKEN_CLASS))
	{
		classDeclaration();
	}
	else if (match(TOKEN_FUN))
	{
		funDeclaration();
	}
	else if (match(TOKEN_VAR))
	{
		varDeclaration();		// declare variable
	}
	else
	{
		statement();
	}
	if (parser.panicMode) synchronize();		// for errors
}

static void statement( )					// either an expression or a print
{

	if (match(TOKEN_PRINT))			
	{
		printStatement();
	}
	else if (match(TOKEN_RETURN))
	{
		returnStatement();			// for functions return
	}
	else if (match(TOKEN_WHILE))
	{
		whileStatement();
	}
	else if (match(TOKEN_FOR))
	{
		forStatement();
	}
	else if (match(TOKEN_SWITCH))
	{
		switchStatement();
	}
	else if (match(TOKEN_IF))
	{
		ifStatement();
	}
	else if (match(TOKEN_LEFT_BRACE))		// parse initial { token
	{
		beginScope();
		block();
		endScope();
	}
	else
	{
		expressionStatement();
	}
}

ObjFunction* compile(const char* source)
{
	initScanner(source);			// start scan/lexing
	Compiler compiler;
	initCompiler(&compiler, TYPE_SCRIPT);

	parser.hadError = false;
	parser.panicMode = false;

	advance();						// call to advance once to 'pump' the scanner
	
	while (!match(TOKEN_EOF))		/// while EOF token is not met
	{
		declaration();
	}

	
	ObjFunction* function = endCompiler();					// ends the expression with a return type
	return parser.hadError ? NULL : function;		// if no error return true
}


// marking compiler roots, for garbage collection
void markCompilerRoots()
{
	Compiler* compiler = current;
	while (compiler != NULL)
	{
		markObject((Obj*)compiler->function);
		compiler = compiler->enclosing;
	}
}
