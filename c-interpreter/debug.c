#include <stdio.h>

#include "debug.h"
#include "value.h"
#include "object.h"


static int simpleInstruction(const char* name, int offset)
{
	printf("%s\n", name);	// print as a string, or char*
	return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset)
{
	uint8_t slot = chunk->code[offset + 1];
	printf("%-16s %4d\n", name, slot);
	return offset + 2;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset)
{
	uint8_t constant = chunk->code[offset + 1];		// pullout the constant index from the subsequent byte in the chunk
	printf("%-16s %4d '", name, constant);			// print out name of the opcode, then the constant index
	printValue(chunk->constants.values[constant]);	//	display the value of the constant,  user defined function
	printf("'\n");
	return offset + 2;			//OP_RETURN is a single byte, and the other byte is the operand, hence offsets by 2
}

static int invokeInstruction(const char* name, Chunk* chunk, int offset)
{
	uint8_t constant = chunk->code[offset + 1];				// get index of the name first
	uint8_t argCount = chunk->code[offset + 2];				// then get number of arguments
	printf("%-16s (%d args) %4d", name, argCount, constant);
	printValue(chunk->constants.values[constant]);			// print the method
	printf("\n");
	return offset + 3;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset)
{
	uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);	// get jump
	jump |= chunk->code[offset + 2];
	printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
	return offset + 3;

}

void disassembleChunk(Chunk* chunk, const char* name)
{
	printf("== %s ==\n", name);				// print a little header for debugging

	for (int offset = 0; offset < chunk->count;)	// for every existing instruction in the chunk
	{	
		offset = disassembleInstruction(chunk, offset);		// disassemble individually, offset will be controlled from this function
	}
}

int disassembleInstruction(Chunk* chunk, int offset)
{
	printf("%04d ", offset);	// print byte offset of the given instruction, or the index
	/* quick note on C placeholders
	say, we have int a = 2
	if %2d, it will be " 2"
	if %02d, it will be "02'
	*/


	// show source line each instruction was compiled from
	if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])		// show a | for any instruction that comes from the 
																			//same source as its preceding one

	{
		printf("	| ");
	}
	else
	{
		printf("%4d ", chunk->lines[offset]);
	}

	uint8_t instruction = chunk->code[offset];		// takes one byte, or an element, from the container
	switch (instruction)
	{
	case OP_CONSTANT:
		return constantInstruction("OP_CONSTANT", chunk, offset);		// pass in chunk to get ValueArray element
	
	// literals
	case OP_NULL:
		return simpleInstruction("OP_NULL", offset);
	case OP_TRUE:
		return simpleInstruction("OP_TRUE", offset);
	case OP_FALSE:
		return simpleInstruction("OP_FALSE", offset);

	case OP_EQUAL:
		return simpleInstruction("OP_EQUAL", offset);
	case OP_GREATER:
		return simpleInstruction("OP_GREATER", offset);
	case OP_LESS:
		return simpleInstruction("OP+LESS", offset);

	// unary
	case OP_NEGATE:
		return simpleInstruction("OP_NEGATE", offset);
	
	// binary
	case OP_ADD:
		return simpleInstruction("OP_ADD", offset);
	case OP_SUBTRACT:
		return simpleInstruction("OP_MINUS", offset);
	case OP_MULTIPLY:
		return simpleInstruction("OP_MULTIPLY", offset);
	case OP_DIVIDE:
		return simpleInstruction("OP_DIVIDE", offset);
	case OP_MODULO:
		return simpleInstruction("OP_MODULO", offset);

	case OP_NOT:
		return simpleInstruction("OP_NOT", offset);

	case OP_POP:
		return simpleInstruction("OP_POP", offset);

	// names for local variables do not get carried over, hence only the slot number is shown
	case OP_GET_LOCAL:
		return byteInstruction("OP_GET_LOCAL", chunk, offset);		
	case OP_SET_LOCAL:
		return byteInstruction("OP_SET_LOCAL", chunk, offset);

	case OP_GET_UPVALUE:
		return byteInstruction("OP_GET_UPVALUE", chunk, offset);
	case OP_SET_UPVALUE:
		return byteInstruction("OP_SET_UPVALUE", chunk, offset);
	case OP_GET_PROPERTY:
		return constantInstruction("OP_GET_PROPERTY", chunk, offset);
	case OP_SET_PROPERTY:
		return constantInstruction("OP_SET_PROPERTY", chunk, offset);

	case OP_CLOSE_UPVALUE:
		return simpleInstruction("OP_CLOSE_VALUE", offset);

	case OP_DEFINE_GLOBAL:
		return simpleInstruction("OP_DEFINE_GLOBAL", offset);
	case OP_GET_GLOBAL:
		return simpleInstruction("OP_GET_GLOBAL", offset);
	case OP_SET_GLOBAL:
		return simpleInstruction("OP_SET_GLOBAL", offset);
	case OP_PRINT:
		return simpleInstruction("OP_PRINT", offset);

	case OP_SWITCH_EQUAL:
		return simpleInstruction("OP_SWITCH_EQUAL", offset);

	case OP_JUMP:
		return jumpInstruction("OP_JUMP", 1, chunk, offset);
	case OP_JUMP_IF_FALSE:
		return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);

	case OP_CALL:
		return byteInstruction("OP_CALL", chunk, offset);

	case OP_METHOD:
		return constantInstruction("OP_METHOD", chunk, offset);

	case OP_INVOKE:
		return invokeInstruction("OP_INVOKE", chunk, offset);


	case OP_CLOSURE:
	{
		offset++;
		uint8_t constant = chunk->code[offset++];			// index for Value
		printf("%-16s %4d ", "OP_CLOSURE", constant);
		printValue(chunk->constants.values[constant]);		// accessing the value using the index
		printf("\n");

		ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
		for (int j = 0; j < function->upvalueCount; j++)	// walk through upvalues
		{
			int isLocal = chunk->code[offset++];
			int index = chunk->code[offset++];
			printf("%04d	|	%s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
		}

		return offset;
	}

	case OP_CLASS:
		return constantInstruction("OP_CLASS", chunk, offset);

	case OP_INHERIT:
		return simpleInstruction("OP_INEHEIRT", offset);


	case OP_GET_SUPER:				// class inheritance
		return constantInstruction("OP_GET_SUPER", chunk, offset);

	case OP_SUPER_INVOKE:
		return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);

	case OP_RETURN:
		return simpleInstruction("OP_RETURN", offset);		// dispatch to a utility function to display it

	case OP_LOOP:
		return jumpInstruction("OP_LOOP", -1, chunk, offset);

	case OP_LOOP_IF_TRUE:
		return jumpInstruction("OP_LOOP_IF_TRUE", -1, chunk, offset);

	case OP_LOOP_IF_FALSE:
		return jumpInstruction("OP_LOOP_IF_FALSE", -1, chunk, offset);

	default:
		printf("Unknown opcode %d\n", instruction);
		return offset + 1;
	}
}


