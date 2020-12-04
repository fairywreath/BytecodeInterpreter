#include <stdarg.h>	// for variadic functions, va_list
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "object.h"
#include "memory.h"
#include "compiler.h"
#include "debug.h"
#include "virtualm.h"

// initialize virtual machine here
VM vm;


// forward declartion of run
static InterpretResult run();

static void resetStack()
{
	// point stackStop to the begininng of the empty array
	vm.stackTop = vm.stack;		// stack array(vm.stack) is already indirectly declared, hence no need to allocate memory for it
}

// IMPORTANT
// variadic function ( ... ), takes a varying number of arguments
static void runtimeError(const char* format, ...)
{
	
	va_list args;	// list from the varying parameter
	va_start(args, format);
	vprintf(format, args);		// unlike book, not vprintf(stderr, format, args)
	va_end(args);
	fputs("\n", stderr);	// fputs; write a string to the stream but not including the null character
	

	// tell which line the error occurred
	size_t instruction = vm.ip - vm.chunk->code - 1;	
	int line = vm.chunk->lines[instruction];
	fprintf(stderr, "Error in script at [Line %d]\n", line);

	resetStack();
}


void initVM()
{
	resetStack();
	vm.objects = NULL;
	initTable(&vm.globals);
	initTable(&vm.strings);
}

void freeVM()
{
	freeObjects();		// free all objects, from vm.objects
	freeTable(&vm.globals);
	freeTable(&vm.strings);
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

// PEEK from the STACK, AFTER the compiler passes it through
// return a value from top of the stack but does not pop it, distance being how far down
// this is a C kind of accessing arrays/pointers
static Value peek(int distance)
{
	return vm.stackTop[-1 - distance];
}

// comparison for OP_NOT
static bool isFalsey(Value value)
{
	// return true if value is the null type or if it is a false bool type
	bool test = IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));

	return test;
}

// string concatenation
static void concatenate()
{
	ObjString* second = AS_STRING(pop());
	ObjString* first = AS_STRING(pop());
	
	int length = first->length + second->length;
	char* chars = ALLOCATE(char, length + 1);		// dynamically allocate memory for the char, chars is now a NULL string

	/* NOTE ON C STRINGS, NULL VS EMPTY
	-> null string has no elements, it is an empty charray, ONLY DECLARED
	-> an empty string has the null character '/0'
	*/

	// IMPORTANt -> use memcpy when assinging to a char* pointer
	memcpy(chars, first->chars, first->length);		// memcpy function, copy to chars, from first->chars, with second->length number of bits
	memcpy(chars + first->length, second->chars, second->length);		// remember to add the first length of bits to chars again, so it will START AFTER the given offset
	chars[length] = '\0';			// IMPORTANT-> terminating character for Cstring, if not put will get n2222

	ObjString* result = takeString(chars, length);		// declare new ObjString ptr
	push(OBJ_VAL(result));
}


/* starting point of the compiler */
InterpretResult interpret(const char* source)
{
	Chunk chunk;			// declare chunk/bytecode for the compiler
	initChunk(&chunk);		// initialize the chunk

	// pass chunk to compiler and fill it with bytecode
	if (!compile(source, &chunk))		// if compilation fails
	{
		freeChunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;			// set vm chunk
	vm.ip = vm.chunk->code;		// assign pointer to the start of the chunk

	InterpretResult result = run();

	freeChunk(&chunk);
	return result;
}


// run the chunk
// most IMPORTANT part of the interpreter
static InterpretResult run()		// static means the scope of the function is only to this file
{

/* info on the macros below
Below macros are FUNCTIONSt that take ZERO arguments, and what is inside () is their return value
READ_BYTE:	
	macro to ACCESS the BYTE(uin8_t) from the POINTER(ip), and increment it
	reads byte currently pointed at ip, then advances the instruction pointer
READ_CONSTANT:
	return constants.values element, from READ_BYTE(), which points exactly to the NEXT index
READ STRING:
	return as object string, read directly from the vm(oip)
*/

#define READ_BYTE() (*vm.ip++)		
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])	
#define READ_STRING() AS_STRING(READ_CONSTANT())

// for patch jumps
// yanks next two bytes from the chunk(used to calculate the offset earlier) and return a 16-bit integer out of it
// use bitwise OR
#define READ_SHORT()	\
	(vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))

// MACRO for binary operations
// take two last constants, and push ONE final value doing the operations on both of them
// this macro needs to expand to a series of statements, read a-virtual-machine for more info, this is a macro trick or a SCOPE BLOCK
// pass in an OPERAOTR as a MACRO
// valueType is a Value struct
// first check that both operands are numbers
#define BINARY_OP(valueType, op)	\
	do {	\
		if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))	\
		{	\
			runtimeError("Operands must be numbers.");	\
			return INTERPRET_RUNTIME_ERROR;		\
		}	\
		double b = AS_NUMBER(pop());	\
		double a = AS_NUMBER(pop());	\
		push(valueType(a op b));	\
	} while(false)	\

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

		// prints every existing value in the stack
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
				// function is smart; chunk advances by 1 on first read, then in the READ_CONSTANT() macro it reads again which advances by 1 and returns the INDEX
				Value constant = READ_CONSTANT();		// READ the next line, which is the INDEX of the constant in the constants array
				push(constant);		// push to stack
				break;			// break from the switch
			}
			// unary opcode
			case OP_NEGATE: 
				if (!IS_NUMBER(peek(0)))		// if next value is not a number
				{
					//printf("\nnot a number\n"); it actually works
					runtimeError("Operand must be a number.");
					return INTERPRET_RUNTIME_ERROR;
				}
				
				push(NUMBER_VAL(-AS_NUMBER(pop()))); 
				break;  // negates the last element of the stack
			
			// literals
			case OP_NULL: push(NULL_VAL); break;
			case OP_TRUE: push(BOOL_VAL(true)); break;
			case OP_FALSE: push(BOOL_VAL(false)); break;

			// binary opcode
			case OP_ADD: 
			{
				if (IS_STRING(peek(0)) && IS_STRING(peek(1)))	// if last two constants are strings
				{
					concatenate();
				}
				else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
				{
					// in the book, macro is not used and a new algorithm is used directly
					BINARY_OP(NUMBER_VAL, +); 		// initialize new Value struct (NUMBER_VAL) here
				}
				else		// handle errors dynamically here
				{
					//printf("operands error");
					runtimeError("Operands are incompatible.");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			
			case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
			case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
			case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;

			case OP_NOT:
				push(BOOL_VAL(isFalsey(pop())));		// again, pops most recent one from the stack, does the operation on it, and pushes it back
				break;

			case OP_EQUAL:		// implemenation comparison done here
			{
				Value b = pop();
				Value a = pop();
				push(BOOL_VAL(valuesEqual(a, b)));
				break;
			}
			case OP_GREATER: BINARY_OP(BOOL_VAL, > ); break;
			case OP_LESS: BINARY_OP(BOOL_VAL, < ); break;


			case OP_PRINT:
			{
				// ACTUAL PRINTING IS DONE HERE
				printValue(pop());		// pop the stack and print the value, getting it from value.c
				printf("\n");
				break;
			}

			case OP_POP: pop(); break;

			case OP_GET_LOCAL:
			{
				uint8_t slot = READ_BYTE();
				push(vm.stack[slot]);			// pushes the value to the stack where later instructions can read it
				break;
			}

			case OP_SET_LOCAL:
			{
				uint8_t slot = READ_BYTE();
				// all the local var's VARIABLES are stored inside vm.stack
				vm.stack[slot] = peek(0);		// takes from top of the stack and stores it in the stack slot
				break;
			}

			case OP_DEFINE_GLOBAL:
			{	
				ObjString* name = READ_STRING();		// get name from constant table
				tableSet(&vm.globals, name, peek(0));	// take value from the top of the stack
				pop();
				break;
			}

			case OP_GET_GLOBAL:
			{
				ObjString* name = READ_STRING();	// get the name
				Value value;		// create new Value
				if (!tableGet(&vm.globals, name, &value))	// if key not in hash table
				{
					runtimeError("Undefined variable '%s'.", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				push(value);
				break;
			}

			case OP_SET_GLOBAL:
			{
				ObjString* name = READ_STRING();
				if (tableSet(&vm.globals, name, peek(0)))	// if key not in hash table
				{
					tableDelete(&vm.globals, name);		// delete the false name 
					runtimeError("Undefined variable '%s'.", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_JUMP:		// will always jump
			{
				uint16_t offset = READ_SHORT();
				vm.ip += offset;
				break;
			}

			case OP_JUMP_IF_FALSE:		// for initial if, will not jump if expression inside is true
			{
				uint16_t offset = READ_SHORT();				// offset already put in the stack
				// actual jump instruction is done here; skip over the instruction pointer
				if (isFalsey(peek(0))) vm.ip += offset;		// if evaluated expression inside if statement is false jump
				break;
			}

			case OP_LOOP:
			{
				uint16_t offset = READ_SHORT();
				vm.ip -= offset;		// jumps back
				break;
			}

			case OP_RETURN:				
			{
				// exit interpreter
				return INTERPRET_OK;
			}
		}
	}


#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}