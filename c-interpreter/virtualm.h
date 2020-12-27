// backend virtual machine to run the code
// a chunk is handed to the virtual machine, and the vm runs the chunk of  code
// vm is stackbased

#ifndef virtualm_h
#define virtualm_h

#include "object.h"
#include "chunk.h"
#include "hasht.h"
#include "value.h"

// max frames is fixed
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)		


// the call stack
// keep track where on the stack a function's local begin, where the caller should resume, etc.
// a call frame represents a single ongoing function call
// each time a function is called, create this struct
typedef struct
{
	ObjClosure* closure;
	uint8_t* ip;		// store ip on where in the VM the function is
	Value* slots;		// this points into the VM's value stack at the first slot the function can use
} CallFrame;

typedef struct
{
	// since the whole program is one big 'main()' use callstacks
	CallFrame frames[FRAMES_MAX];		
	int frameCount;				// stores current height of the stack

	Value stack[STACK_MAX];			// stack array is 'indirectly' declared inline here
	Value* stackTop;			// pointer to the element just PAST the element containing the top value of the stack

	Table globals;		// for storing global variables
	Table strings;		// for string interning, to make sure every equal string takes one memory

	ObjString* initString;			// init string for class constructors

	ObjUpvalue* openUpvalues;		// track all upvalues; points to the first node of the linked list

	Obj* objects;		// pointer to the header of the Obj itself/node, start of the list
						// nicely used in GARBAGE COLLECTION, where objects are nicely erased in the middle

	// stack to store gray marked Objects for garbage collection
	int grayCapacity;		
	int grayCount;
	Obj** grayStack;			// array of pointers pointing to a particular subgraph

	// self-adjusting-g-heap, to control frequency of GC, bytesAllocated is the running total
	size_t bytesAllocated;		// size_t is a 32 bit(integer/4bytes), represents size of an object in bytes
	size_t nextGC;				// threhsold that triggers the GC
} VM;

// rseult that responds from the running VM
typedef enum
{
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;


void initVM();
void freeVM();

// interpret/run chunks and return enum
// changed from interpreting chunks to interpreting strings
InterpretResult interpret(const char* source);

extern VM vm;		// use extern, declare as global variable

// stack operations to determine order of value manipulations
void push(Value value);
Value pop();

#endif