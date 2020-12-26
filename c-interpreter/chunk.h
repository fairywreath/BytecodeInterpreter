// 'chunks' of bytecode
#ifndef chunk_h
#define chunk_h

#include "common.h"
#include "value.h"

// in bytecode format, each instruction has a one-byte operation code(opcode)
// the number controls what kind of instruction we're dealing with- add, subtract, etc
// typedef enums are bytes apparently
// these are INSTRUCTIONS 
typedef enum
{
	OP_CONSTANT,	// chunk needs to know when to produce constants and print them in the right order
					// they have operands, to eg. identify which variable to load
					// OP_CONSTANT take up 2 bytes, one is the opcode itself and the other the constant index
	
	OP_NULL,
	OP_TRUE,
	OP_FALSE,

	// unary operators
	OP_NEGATE,		// operand to negate, utilized in virtual machine

	// literals/declarations
	OP_PRINT,
	OP_POP,			// basically pops a value off the stack and forgets it, used for expression statements
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_DEFINE_GLOBAL,
	OP_GET_GLOBAL,
	OP_SET_GLOBAL,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,
	OP_GET_PROPERTY,
	OP_SET_PROPERTY,
	
	// binary operators
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_MODULO,

	// logical, unary
	OP_NOT,
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,

	OP_SWITCH_EQUAL,
	OP_CLOSE_UPVALUE,

	OP_JUMP,
	OP_JUMP_IF_FALSE,		// takes a 16-bit operand
	OP_CALL,

	OP_LOOP, 
	OP_LOOP_IF_FALSE,		// repeat until
	OP_LOOP_IF_TRUE,	// do while

	OP_CLOSURE,
	OP_CLASS,
	OP_METHOD,
	OP_INVOKE,

	OP_INHERIT,			// class inheritance
	OP_GET_SUPER,		// for superclasses
	OP_SUPER_INVOKE,

	OP_RETURN,		// means return from current function
} OpCode;			// basically a typdef call to an enum
					// in C, you cannot have enums called simply by their rvalue 'string' names, use typdef to define them


/* dynamic array for bytecode */
// btyecode is a series of instructions, this is a struct to hold instructions
// create own dynamic array
typedef struct
{
	int count;					// current size
	int capacity;				// max array size
	uint8_t* code;				// 1 byte unsigned int, to store the CODESTREAM
	int* lines;					// array of integers that parallels the bytecode/codestream, to get where each location of the bytecode is
	ValueArray constants;		// store double value literals
} Chunk;
		
void initChunk(Chunk* chunk);		// initialize array
void freeChunk(Chunk* chunk);		// free/delete chunk and then restart with an empty chunk
void writeChunk(Chunk* chunk, uint8_t byte, int line);	// add to array
														// when we write a byte of code to the chunk, need to know source line it came from
// add explicit function to add constats
int addConstant(Chunk* chunk, Value value);


/* the top two are simply wrapper around bytes */


#endif