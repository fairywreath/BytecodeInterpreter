#include <stdarg.h>	// for variadic functions, va_list
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "object.h"
#include "memory.h"
#include "compiler.h"
#include "debug.h"
#include "virtualm.h"

// initialize virtual machine here
VM vm;

static Value clockNative(int argCount, Value* args)
{
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);		// returns elapsed time since program was running
}

// forward declartion of run
static InterpretResult run();

static void resetStack()
{
	// point stackStop to the begininng of the empty array
	vm.stackTop = vm.stack;		// stack array(vm.stack) is already indirectly declared, hence no need to allocate memory for it
	vm.frameCount = 0;
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
	

	// printing the stack trace for the function
	// print out each function that was still executing when the program died and where the execution was at the point it died
	for (int i = vm.frameCount - 1; i >= 0; i--)
	{
		CallFrame* frame = &vm.frames[i];
		ObjFunction* function = frame->closure->function;
		// - 1 because IP is sitting on the NEXT INSTRUCTION to be executed
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
		if (function->name == NULL)
		{
			fprintf(stderr, "script\n");
		}
		else
		{
			fprintf(stderr, "%s(%d)\n", function->name->chars, function->arity);
		}

	}


	// tell which line the error occurred
	CallFrame* frame = &vm.frames[vm.frameCount - 1];		// pulls from topmost CallFrame on the stack
	size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;	// - 1 to deal with the 1 added initially for the main() CallFrame
	int line = frame->closure->function->chunk.lines[instruction];
	fprintf(stderr, "Error in script at [Line %d]\n", line);

	resetStack();
}

static void defineNative(const char* name, NativeFn function)
{
	push(OBJ_VAL(copyString(name, (int)strlen(name))));			// strlen to get char* length
	push(OBJ_VAL(newNative(function)));
	tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
	pop();
	pop();
}


void initVM()
{
	resetStack();
	vm.objects = NULL;
	initTable(&vm.globals);
	initTable(&vm.strings);

	defineNative("clock", clockNative);
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


/* for call stacks/functions  */
static bool call(ObjClosure* closure, int argCount)
{

	if (argCount != closure->function->arity)	// if number of parameters does not match
	{
		runtimeError("Expected %d arguments but got %d", closure->function->arity, argCount);
		return false;
	}

	// as CallFrame is an array, to ensure array does not overflow
	if (vm.frameCount == FRAMES_MAX)
	{
		runtimeError("Stack overflow.");
		return false;
	}

	// get pointer to next in frame array
	CallFrame* frame = &vm.frames[vm.frameCount++];			// initializes callframe to the top of the stack
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;

	// set up slots pointer to give frame its window into the stack
	// ensures everyting lines up
	// slots is the 'starting pointer' for the function cll
	frame->slots = vm.stackTop - argCount - 1;
	return true;
}

static bool callValue(Value callee, int argCount)
{
	if (IS_OBJ(callee))
	{
		switch (OBJ_TYPE(callee))
		{
		case OBJ_CLOSURE:				// ensure type is function
			return call(AS_CLOSURE(callee), argCount);		// call to function happens here

		case OBJ_NATIVE:
		{
			NativeFn native = AS_NATIVE(callee);
			Value result = native(argCount, vm.stackTop - argCount);
			vm.stackTop -= argCount + 1;				// remove call and arguments from the stack
			push(result);
			return true;
		}
		default:	// non callable
			break;	
		}
	}
}

// get corresponding upvalue 
static ObjUpvalue* captureUpvalue(Value* local)
{
	ObjUpvalue* createdUpvalue = newUpvalue(local);
	return createdUpvalue;
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
	ObjFunction* function = compile(source);
	if (function == NULL) return INTERPRET_COMPILE_ERROR;		// NULL gets passed from compiler

	push(OBJ_VAL(function));
	ObjClosure* closure = newClosure(function);
	pop();
	push(OBJ_VAL(closure));
	callValue(OBJ_VAL(closure), 0);			// 0 params for main()


	return run();
}


// run the chunk
// most IMPORTANT part of the interpreter
static InterpretResult run()		// static means the scope of the function is only to this file
{
	CallFrame* frame = &vm.frames[vm.frameCount - 1];

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

#define READ_BYTE() (*frame->ip++)		
#define READ_CONSTANT()		\
	(frame->closure->function->chunk.constants.values[READ_BYTE()])	
#define READ_STRING() AS_STRING(READ_CONSTANT())

// for patch jumps
// yanks next two bytes from the chunk(used to calculate the offset earlier) and return a 16-bit integer out of it
// use bitwise OR
#define READ_SHORT()	\
	(frame->ip += 2, \
	(uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

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

		
		disassembleInstruction(&frame->closure->function->chunk, 
			(int)(frame->ip - frame->closure->function->chunk.code));
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

			// for switch eqal
			case OP_SWITCH_EQUAL:
			{
				Value b = pop();		// only pop second value
				Value a = peek(0);		// peek topmost, the first value
				push(BOOL_VAL(valuesEqual(a, b)));
				break;
			}

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
				push(frame->slots[slot]);			// pushes the value to the stack where later instructions can read it
				break;
			}

			case OP_SET_LOCAL:
			{
				uint8_t slot = READ_BYTE();
				// all the local var's VARIABLES are stored inside vm.stack
				frame->slots[slot] = peek(0);		// takes from top of the stack and stores it in the stack slot
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

			// upvalues set/get
			case OP_GET_UPVALUE:
			{
				uint8_t slot = READ_BYTE();		// read index
				push(*frame->closure->upvalues[slot]->location);		// push the value to the stack
				break;
			}

			case OP_SET_UPVALUE:
			{
				uint8_t slot = READ_BYTE();		// read index
				*frame->closure->upvalues[slot]->location = peek(0);		// set to the topmost stack
				break;
			}

			case OP_JUMP:		// will always jump
			{
				uint16_t offset = READ_SHORT();
				frame->ip += offset;
				break;
			}

			case OP_JUMP_IF_FALSE:		// for initial if, will not jump if expression inside is true
			{
				uint16_t offset = READ_SHORT();				// offset already put in the stack
				// actual jump instruction is done here; skip over the instruction pointer
				if (isFalsey(peek(0))) frame->ip += offset;		// if evaluated expression inside if statement is false jump
				break;
			}

			case OP_LOOP:
			{
				uint16_t offset = READ_SHORT();
				frame->ip -= offset;		// jumps back
				break;
			}

			// a callstack to a funcion has the form of function name, param1, param2...
			// the top level code, or caller, also has the same function name, param1, param2... in the right order
			case OP_CALL:
			{
				int argCount = READ_BYTE();
				if (!callValue(peek(argCount), argCount))	// call function; pass in the function name istelf[peek(depth)] and the number of arguments
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];			// to update pointer if callFrame is successful, asnew frame is added
				break;
			}

			// closures
			case OP_CLOSURE:
			{
				ObjFunction* function = AS_FUNCTION(READ_CONSTANT());		// load compiled function from table
				ObjClosure* closure = newClosure(function);
				push(OBJ_VAL(closure));

				// fill upvalue array over in the interpreter when a closure is created
				// to see upvalues in each slot
				for (int i = 0; i < closure->upvalueCount; i++)
				{
					uint8_t isLocal = READ_BYTE();		// read isLocal bool
					uint8_t index = READ_BYTE();		// read index for local, if available, in the closure
					if (isLocal)
					{
						closure->upvalues[i] = captureUpvalue(frame->slots + index);		// get from slots stack

					}
					else				// if not local(nested upvalue)
					{
						closure->upvalues[i] = frame->closure->upvalues[index];				// get from current upvalue
					}
				}


				break;
			}

			case OP_RETURN:				
			{
				Value result = pop();	// if function returns a value, value will beon top of the stack

				vm.frameCount--;
				if (vm.frameCount == 0)		// return from 'main()'/script function
				{
					pop();						// pop main script function from the stack
					return INTERPRET_OK;
				}

				// for a function
				// discard all the slots the callee was using for its parameters
				vm.stackTop = frame->slots;		// basically 're-assign'
				push(result);		// push the return value

				frame = &vm.frames[vm.frameCount - 1];		// update run function's current frame
				break;
			}
		}
	}


#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}