#include "common.h"
#include "debug.h"
#include "virtualm.h"
#include <stdio.h>

// initialize virtual machine here
VM vm;

static void resetStack()
{
	// point stackStop to the begininng of the empty array
	vm.stackTop = vm.stack;		// stack array(vm.stack) is already indirectly declared, hence no need to allocate memory for it
}

void initVM()
{
	resetStack();
}

void freeVM()
{

}

/* stack operations */
void push(Value value)
{
	*vm.stackTop = value;		// * in front of the pointer means the rvalue itself, assign value(parameter) to it
	vm.stackTop++;
}

Value pop()
{
	vm.stackTop--;		// first move the stack BACK to get the last element(stackTop points to ONE beyond the last element)
	return  *vm.stackTop;
}
/* end of stack operations */

InterpretResult interpret(Chunk* chunk)
{
	vm.chunk = chunk;				// vm object above
	vm.ip = vm.chunk->code;			// vm works by running through the bytecode, to keep track of where it is, we use a uint8_t array
									// vm.ip store the location of the current insruction
									// this is a BTYE POINTER; we point it to the NEXT code of instruction(not the code it is currently running on)


	return run();
}


// run the chunk
static InterpretResult run()		// static means the scope of the function is only to this file
{

/* info on the macros below
Below macros are FUNCTIONSt that take ZERO arguments, and what is inside () is their return value
READ_BYTE:	
	macro to ACCESS the BYTE(uin8_t) from the POINTER(ip), and increment it
	reads byte currently pointed at ip, then advances the instruction pointer
READ_CONSTANT:
	return constants.values element, from READ_BYTE(), which points exactly to the NEXT index
*/

#define READ_BYTE() (*vm.ip++)		
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])	

	for (;;)
	{
	// disassembleInstruction needs an byte offset, do pointer math to convert ip back to relative offset
	// from the beginning of the chunk (subtract current ip from the starting ip)
	// IMPORTANT -> only for debugging the VM
#ifdef DEBUG_TRACE_EXECUTION
		// for stack tracing
		printf("		");
		/* note on C POINTERSE
		-> pointing to the array itself means pointing to the start of the array, or the first element of the array
		-> ++/-- means moving through the array (by 1 or - 1)
		-> you can use operands like < > to tell compare how deep are you in the array
		*/

		for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
		{
			printf("[ ");
			printValue(*slot);
			printf(" ]");
		}
		disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
		uint8_t instruction;
		switch (instruction = READ_BYTE())			// get result of the byte read, every set of instruction starts with an opcode
		{
			case OP_CONSTANT: 
			{
				Value constant = READ_CONSTANT();
				push(constant);		// push to stack
				break;			// break from the switch
			}
			case OP_NEGATE: 
			{
				push(-pop());		// negates the last element of the stack
				break;
			}
			case OP_RETURN:				
			{
				// ACTUAL PRINTING IS DONE HERE
				printValue(pop());		// pop the stack and print the value
				printf("\n");
				return INTERPRET_OK;
			}
		}
	}


#undef READ_BYTE
#undef READ_CONSTANT
}