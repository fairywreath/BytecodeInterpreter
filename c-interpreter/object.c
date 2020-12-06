#include <stdio.h>
#include <string.h>		//memcpy


#include "memory.h"
#include "object.h"
#include "hasht.h"
#include "value.h"
#include "virtualm.h"


/* copying the string from the const char* in the source code to the heap 
-> does not reuse string pointers from the source code
*/

// macro to avoid redundantly cast void* back to desired type
#define ALLOCATE_OBJ(type, objectType)	\
	(type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type)
{
	Obj* object = (Obj*)reallocate(NULL, 0, size);		// allocate memory for obj
	object->type = type;
	
	// every time an object is allocated, insert to the list
	// insert as the HEAD; the latest one inserted will be at the start
	object->next = vm.objects;			// vm from virtualm.h, with extern
	vm.objects = object;		

	return object;
}

// create new closure
ObjClosure* newClosure(ObjFunction* function)
{
	// initialize array of upvalue pointers
	// upvalues carry over
	ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);

	for (int i = 0; i < function->upvalueCount; i++)
	{
		upvalues[i] = NULL;				// initialize all as null
	}


	ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;
	return closure;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash)			// pass in hash
{
	ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length = length;
	string->chars = chars;
	string->hash = hash;

	//printf("allocate\n");
	tableSet(&vm.strings, string, NULL_VAL);		// for string interning

	return string;
}


ObjFunction* newFunction()
{
	ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);

	function->arity = 0;
	function->upvalueCount = 0;
	function->name = NULL;
	initChunk(&function->chunk);
	return function;
}

// new native function
ObjNative* newNative(NativeFn function)
{
	ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
	native->function = function;
	return native;
}



// hash function, the FNV-1a
static uint32_t hashString(const char* key, int length)
{
	uint32_t hash = 2116136261u;		// initial hash value, u at end means unsigned

	for (int i = 0; i < length; i++)	// traverse through the data to be hashed
	{
		hash ^= key[i];			// munge the bits from the string key to the hash value; ^= is a bitwise operator
		hash *= 16777619;
	}

	return hash;
}


// shorten than copyString because owernship of the char* itself is declared in concatenate(), hence no need to declare memory again
ObjString* takeString(char* chars, int length)
{
	uint32_t hash = hashString(chars, length);
	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);


	if (interned != NULL)		// if the same string already exists
	{
		FREE_ARRAY(char, chars, length + 1);		// free the memory for use
		return interned;
	}


	return allocateString(chars, length, hash);
}

// copy string from source code to memory
ObjString* copyString(const char* chars, int length)
{
	uint32_t hash = hashString(chars, length);
	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);

	if (interned != NULL) {
		return interned;	// if we find a string already in vm.srings, no need to copy just return the pointer
	}
	char* heapChars = ALLOCATE(char, length + 1);	// length +1 for null terminator
	memcpy(heapChars, chars, length);			// copy memory from one location to another; memcpy(*to, *from, size_t (from))
	heapChars[length] = '\0';		// '\0', a null terminator used to signify the end of the string, placed at the end

	return allocateString(heapChars, length, hash);
}


ObjUpvalue* newUpvalue(Value* slot)
{
	ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
	upvalue->location = slot;
	return upvalue;
}

static void printFunction(ObjFunction* function)
{
	if (function->name == NULL)
	{
		printf("<script>");
		return;
	}
	printf("fun %s(%d params)", function->name->chars, function->arity);		// print name and number of parameters
}

void printObject(Value value)
{
	// first class objects can be printed; string and functions
	switch (OBJ_TYPE(value))
	{
	case OBJ_CLOSURE:
		printFunction(AS_CLOSURE(value)->function);
		break;
	case OBJ_FUNCTION:
		printFunction(AS_FUNCTION(value));
		break;
	case OBJ_NATIVE:
		printf("<native fun>");
		break;
	case OBJ_STRING:
		printf("%s", AS_CSTRING(value));
		break;
	case OBJ_UPVALUE:
		printf("upvalue");
		break;
	default:
		return;
	}
}