#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "virtualm.h"

int main(int argc, const char* argv[])		// used in the command line, argc being the amount of arguments and argv the array
{
	initVM();
	Chunk chunk;
	initChunk(&chunk);
	
	// add constant
	int constant = addConstant(&chunk, 1.2);	// add value to constant pool. which returns the index of the constant array
	writeChunk(&chunk, OP_CONSTANT, 123);			// add the constant instruction, starting with the opcode
												// basically bringing the constant from BEHIND(the valuearray) to the codestream
	writeChunk(&chunk, constant, 123);				// write the one-byte constant index operand
	writeChunk(&chunk, OP_NEGATE, 123);

	writeChunk(&chunk, OP_RETURN, 123);		// add OP_RETURN enum to chunk

	// create own diassembler, to go from blob of machine code to output(eg. terminal)

	// debug the BACKEND/CORE of the bytecode interpreter
	disassembleChunk(&chunk, "test chunk");

	// interpreting the chunk/writing the code
	// to debug the VIRTUAL MACHINE
	interpret(&chunk);

	freeVM();
	freeChunk(&chunk);

	return 0;
}