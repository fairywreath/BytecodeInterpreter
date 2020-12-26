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
	vm.openUpvalues = NULL;
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
	resetStack();			// initialiing the Value stack, also initializing the callframe count
	vm.objects = NULL;
	initTable(&vm.globals);
	initTable(&vm.strings);

	// initializing gray marked obj stack for garbage collection
	vm.grayCapacity = 0;
	vm.grayCount = 0;
	vm.grayStack = NULL;

	// self adjusting heap to control frequency of GC
	vm.bytesAllocated = 0;
	vm.nextGC = 1024 * 1024;


	// init initalizer string
	vm.initString = NULL;
	vm.initString = copyString("init", 4);

	defineNative("clock", clockNative);
}

void freeVM()
{
	vm.initString = NULL;
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
		case OBJ_BOUND_METHOD:
		{
			ObjBoundMethod* bound = AS_BOUND_METHOD(callee);		// get ObjBoundMethod from value type(callee)
			vm.stackTop[-argCount - 1] = bound->receiver;		// set [-] inside square brackes of top stack pointer to go down the stack
			return call(bound->method, argCount);			//	run call to execute
		}
		case OBJ_CLASS:		// create class instance
		{
			ObjClass* kelas = AS_CLASS(callee);
			// create new instance here
			vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(kelas));		// - argcounts as above values are parameters

			// initializer
			Value initializer;
			// if we find one from the table
			if (tableGet(&kelas->methods, vm.initString, &initializer))			// have a vm.initString as 'token', ObjString type	
			{
				return call(AS_CLOSURE(initializer), argCount);
			}
			else if (argCount != 0)   // if there ARE arguments but the initalizer method cannot be found
			{
				runtimeError("Expected 0  arguments but got %d\n", argCount);
				return false;
			}

			return true;
		}
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


static bool invokeFromClass(ObjClass* kelas, ObjString* name, int argCount)
{
	Value method;
	if (!tableGet(&kelas->methods, name, &method))
	{
		runtimeError("Undefined property '%s'.", name->chars);
		return false;
	}

	return call(AS_CLOSURE(method), argCount);
}




// invoke class method, access method + call method
static bool invoke(ObjString* name, int argCount)
{
	Value receiver = peek(argCount);		// grab the receiver of the stack

	// call method with wrong type, not an objinstance type
	if (!IS_INSTANCE(receiver))
	{
		runtimeError("Tried to invoke a method from a non instance object.");
		return false;
	}

	ObjInstance* instance = AS_INSTANCE(receiver);

	// for fields()
	Value value;
	if (tableGet(&instance->fields, name, &value))
	{
		vm.stackTop[-argCount - 1] = value;
		return callValue(value, argCount);
	}



	return invokeFromClass(instance->kelas, name, argCount);		// actual function that searches for method and calls it
}



// bind method and wrap it in a new ObjBoundMethod
static bool bindMethod(ObjClass* kelas, ObjString* name)
{
	Value method;			
	if (!tableGet(&kelas->methods, name, &method))			// get method from table and bind it
	{
		// if method not found
		runtimeError("Undefined property %s.", name->chars);
		return false;
	}
	ObjBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method));		// wrap method in a new ObjBoundMethodd

	pop();		// pop the class instance
	push(OBJ_VAL(bound));
	return true;
}


// get corresponding upvalue 
static ObjUpvalue* captureUpvalue(Value* local)
{
	// set up the linked list
	ObjUpvalue* prevUpvalue = NULL;
	ObjUpvalue* upvalue = vm.openUpvalues;		// assign at the start of the list

	// look for an existing upvalue in the list
	/*  LINKED LIST
	1. start at the head of the list, which is the upvalue CLOSET to the TOP OF THE STACK
	2. walk through the list, using a little pointer comparison to iterate past every upvalue pointing
		to slots ABOVE the one we are looking for
	-- upvalue->location (array of the indexes for the locals) is the stack
	
	THREE ways to exit the loop:
	1. local slot stopped is the slot we're looking for
	2. ran ouf ot upvalues to search
	3. found an upvalue whose local slot is BELOW the one we're looking for
	*/
	while (upvalue != NULL && upvalue->location > local)	// pointer comparison: only find the ones ABOVE local
	{
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}

	if (upvalue != NULL && upvalue->location == local)		// if the location/local/indeces match
	{
		return upvalue;				// return already created upvalue
	}

	ObjUpvalue* createdUpvalue = newUpvalue(local);
	createdUpvalue->next = upvalue;			// insert at the front
	
	if (prevUpvalue == NULL)	// ran out of values to search
	{
		vm.openUpvalues = createdUpvalue;			// set pointer to the newly added upvalue
	}
	else			// found local slot BELOW the one we are looking for
	{
		prevUpvalue->next = createdUpvalue;				// link next slot(the value below) to the newly inserted upvalue
	}
	
	return createdUpvalue;
}

// closes every upvalue it can find that points to the slot or any above the stack
static void closeUpvalues(Value* last)			// takes pointer to stack slot
{
	while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last)
	{
		ObjUpvalue* upvalue = vm.openUpvalues;	// pointer to list of openupvalues
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		vm.openUpvalues = upvalue->next;
	}
}

// defining method for class type
static void defineMethod(ObjString* name)
{
	Value method = peek(0);				// method/closure is at the top of the stack
	ObjClass* kelas = AS_CLASS(peek(1));	// class is at the 2nd top
	tableSet(&kelas->methods, name, method);	// add to hashtable
	pop();				// pop the method
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
	ObjString* second = AS_STRING(peek(0));				// peek, so we do not pop it off if calling a GC is needed
	ObjString* first = AS_STRING(peek(1));
	
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
	pop();			// pop the two strings, garbage collection
	pop();		
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

#define MODULO_OP(valueType, op)	\
	do	\
	{	\
			if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1)))	\
		{	\
			runtimeError("Operands must be numbers.");	\
			return INTERPRET_RUNTIME_ERROR;		\
		}	\
	int b = (int)AS_NUMBER(pop());	\
	int a = (int)AS_NUMBER(pop());	\
	push(valueType(a op b));	\
	} while (false)	\


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

			case OP_MODULO: MODULO_OP(NUMBER_VAL, %); break;

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
			
			case OP_GET_PROPERTY:
			{
				// to make sure only instances are allowed to have fields
				if (!IS_INSTANCE(peek(0)))
				{
					runtimeError("Only instances have properties.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjInstance* instance = AS_INSTANCE(peek(0));		// get instance from top most stack
				ObjString* name = READ_STRING();					// get identifier name

				Value value;		// set up value to add to the stack
				if (tableGet(&instance->fields, name, &value))		// get from fields hash table, assign it to instance
				{
					pop();		// pop the instance itself
					push(value);
					break;
				}
				
				if (!bindMethod(instance->kelas, name))		// no method as well, error
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SET_PROPERTY:
			{
				if (!IS_INSTANCE(peek(1)))		// if not an instance
				{
					runtimeError("Identifier must be a class instance.");
					return INTERPRET_RUNTIME_ERROR;
				}

				// not top most, as the top most is reserved for the new value to be set
				ObjInstance* instance = AS_INSTANCE(peek(1));		
				tableSet(&instance->fields, READ_STRING(), peek(0));		//peek(0) is the new value

				Value value = pop();		// pop the already set value
				pop();		// pop the property instance itself
				push(value);		// push the value back again
				break;	
			}



			case OP_CLOSE_UPVALUE:
			{
				closeUpvalues(vm.stackTop - 1);		// put address to the slot
				pop();			// pop from the stack
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

			case OP_LOOP_IF_FALSE:
			{
				uint16_t offset = READ_SHORT();				// offset already put in the stack
				// bool state is at the top of the stack
				// if false loop back
				if (isFalsey(peek(0))) frame->ip -= offset;
				pop();			// pop the true/false
				break;
			}

			case OP_LOOP_IF_TRUE:
			{
				uint16_t offset = READ_SHORT();				// offset already put in the stack
				// bool state is at the top of the stack
				// if not false loop back
				if (!isFalsey(peek(0))) frame->ip -= offset;
				pop();			// pop the true/false
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

			case OP_CLASS:
				push(OBJ_VAL(newClass(READ_STRING())));			// load string for the class' name and push it onto the stack
				break;

			case OP_METHOD:
				defineMethod(READ_STRING());		// get name of the method
				break;

			case OP_INVOKE:
			{
				ObjString* method = READ_STRING();
				int argCount = READ_BYTE();
				if (!invoke(method, argCount))		// new invoke function
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}

			case OP_INHERIT:
			{
				Value parent = peek(1);		// parent class from 2nd top of the stack

				// ensure that parent identifier is a class
				if (!IS_CLASS(parent))
				{
					runtimeError("Parent identifier is not a class.");
					return INTERPRET_RUNTIME_ERROR;
				}

				ObjClass* child = AS_CLASS(peek(0));		// child class at the top of the stack
				tableAddAll(&AS_CLASS(parent)->methods, &child->methods);	// add all methods from parent to child table
				pop();				// pop the child class
				break;
			}

			case OP_GET_SUPER:
			{
				ObjString* name = READ_STRING();		// get method name/identifier
				ObjClass* parent = AS_CLASS(pop());		// class identifier is at the top of the stack
				if (!bindMethod(parent, name))			// if binding fails
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}

			case OP_SUPER_INVOKE:		// super calls optimization
			{
				ObjString* method = READ_STRING();
				int count = READ_BYTE();
				ObjClass* parent = AS_CLASS(pop());
				if (!invokeFromClass(parent, method, count))
				{
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &vm.frames[vm.frameCount - 1];
				break;
			}

			case OP_RETURN:				
			{
				Value result = pop();	// if function returns a value, value will beon top of the stack

				closeUpvalues(frame->slots);   // close lingering closed values

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