#include <stdlib.h>

#include "memory.h"
#include "virtualm.h"
#include "compiler.h"

// for garbage collector debugging
#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

// growth factor for garbage collection heap
#define GC_HEAP_GROW_FACTOR 2

// A void pointer is a pointer that has no associated data type with it.
// A void pointer can hold address of any type and can be typcasted to any type.
void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
	vm.bytesAllocated += newSize - oldSize;		// self adjusting heap for garbage collection

	if (newSize > oldSize)		// when allocating NEW memory, not when freeing as collecGarbage will cal void* reallocate itself
	{
#ifdef DEBUG_STRESS_GC
		collectGarbage();
#endif
	
		// run collecter if bytesAllocated is above threshold
		if (vm.bytesAllocated > vm.nextGC)
		{
			collectGarbage();
		}
	
	}

	if (newSize == 0)
	{
		free(pointer);
		return NULL;
	}

	// C realloc
	void* result = realloc(pointer, newSize);

	// if there is not enought memory, realloc will return null
	if (result == NULL) exit(1);	// exit with code 1

	return result;
}


// you can pass in a'lower' struct pointer, in this case Obj*, and get the higher level which is ObjFunction
void freeObject(Obj* object)		// to handle different types
{
#ifdef DEBUG_LOG_GC
	printf("%p free type %d\n", (void*)object, object->type);
#endif

	switch (object->type)
	{
	case OBJ_BOUND_METHOD:
		FREE(ObjBoundMethod, object);
		break;
	
	case OBJ_CLASS:
	{
		// free class type
		ObjClass* kelas = (ObjClass*)object;
		freeTable(&kelas->methods);
		FREE(ObjClass, object);
		break;
	}
	case OBJ_INSTANCE:
	{
		ObjInstance* instance = (ObjInstance*)object;
		freeTable(&instance->fields);
		FREE(ObjInstance, object);
		break;
	}
	case OBJ_CLOSURE:
	{
		// free upvalues
		ObjClosure* closure = (ObjClosure*)object;
		FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);		
		
		FREE(ObjClosure, object);		// only free the closure, not the function itself
		break;
	}
	case OBJ_FUNCTION:		// return bits(chunk) borrowed to the operating syste,
	{
		ObjFunction* function = (ObjFunction*)object;
		freeChunk(&function->chunk);
		FREE(ObjFunction, object);
		break;
	}
	case OBJ_NATIVE:
	{
		FREE(ObjNative, object);
		break;
	}
	case OBJ_STRING: 
	{
		ObjString* string = (ObjString*)object;
		FREE_ARRAY(char, string->chars, string->length + 1);
		FREE(ObjString, object);
		break;
	}
	case OBJ_UPVALUE:
	{
		FREE(ObjUpvalue, object);
		break;
	}
	}
}

/*		garbage collection		 */	

void markObject(Obj* object)
{
	if (object == NULL) return;				// in some places the pointer is empty
	if (object->isMarked) return;			// object is already marked
	
	object->isMarked = true;

	// create a worklist of grayobjects to traverse later, use a stack to implement it
	if (vm.grayCapacity < vm.grayCount + 1)			// if need more space, allocate
	{	
		vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
		vm.grayStack = realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);			// use native realloc here
	}

	if (vm.grayStack == NULL) exit(1);			// if fail to allocate memory for the gray stack	

	// add the 'gray' object to the working list
	vm.grayStack[vm.grayCount++] = object;

#ifdef DEBUG_LOG_GC
	printf("%p marked ", (void*)object);
	printValue(OBJ_VAL(object));		// you cant print first class objects, like how you would print in the actual repl
	printf("\n");
#endif
}

void markValue(Value value)
{
	if (!IS_OBJ(value)) return;		// if value is not first class Objtype return
	markObject(AS_OBJ(value));
}


// marking array of values/constants of a function, used in blackenObject, case OBJ_FUNCTION
static void markArray(ValueArray* array)
{
	for (int i = 0; i < array->count; i++)
	{
		markValue(array->values[i]);			// mark each Value in the array
	}
}


static void markRoots()
{
	// assiging a pointer to a full array means assigning the pointer to the FIRST element of that array
	for (Value* slot = vm.stack; slot < vm.stackTop; slot++)		// walk through all values/slots in the Value* array
	{
		markValue(*slot);
	}

	// mark closures
	for (int i = 0; i < vm.frameCount; i++)
	{
		markObject((Obj*)vm.frames[i].closure);			// mark ObjClosure  type
	}

	// mark upvalues, walk through the linked list of upvalues
	for (ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next)
	{
		markObject((Obj*)upvalue);
	}


	markTable(&vm.globals);			// mark global variables, belongs in the VM/hashtable

	// compiler also grabs memory; special function only for 'backend' processes
	markCompilerRoots();		// declared in compiler.h
}


// actual tracing of each gray object and marking it black
static void blackenObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
	printf("%p blackened ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("\n");
#endif


	switch (object->type)
	{
	case OBJ_BOUND_METHOD:
	{
		ObjBoundMethod* bound = (ObjBoundMethod*)object;
		markValue(bound->receiver);
		markObject((Obj*)bound->method);
		break;
	}

	case OBJ_UPVALUE:		// simply mark the closed value
		markValue(((ObjUpvalue*)object)->closed);
		break;

	case OBJ_FUNCTION:		// mark the name and its value array of constants
	{
		// you can get the coressponding 'higher' object type from a lower derivation struct in C using (higher*)lower
		ObjFunction* function = (ObjFunction*)object;		
		markObject((Obj*)function->name);		// mark its name, an ObjString type
		markArray(&function->chunk.constants);		// mark value array of chunk constants, pass it in AS A POINTER using &
		break;
	}

	case OBJ_CLOSURE:				// mark the function and all of the closure's upvalues
	{
		ObjClosure* closure = (ObjClosure*)object;
		markObject((Obj*)closure->function);
		for (int i = 0; i < closure->upvalueCount; i++)
		{
			markObject((Obj*)closure->upvalues[i]);
		}
		break;
	}

	case OBJ_CLASS:
	{
		ObjClass* kelas = (ObjClass*)object;
		markObject((Obj*)kelas->name);
		markTable(&kelas->methods);
		break;
	}

	case OBJ_INSTANCE:
	{
		ObjInstance* instance = (ObjInstance*)object;
		markObject((Obj*)instance->kelas);
		markTable(&instance->fields);
		break;
	}
		// these two objects contain NO OUTGOING REFERENCES there is nothing to traverse
	case OBJ_NATIVE:
	case OBJ_STRING:
		break;
	}
}


// traversing the gray stack work list
static void traceReferences()
{
	while (vm.grayCount > 0)
	{
		// pop Obj* (pointer) from the stack
		// note how -- is the prefix; subtract first then use it as an index
		// --vm.grayCount already decreases its count, hence everything is already 'popped'
		Obj* object = vm.grayStack[--vm.grayCount];			
		blackenObject(object);
	}
}


// sweeping all unreachable values
static void sweep()
{
	Obj* previous = NULL;
	Obj* object = vm.objects;		// linked intrusive list of Objects in the VM
	
	while (object != NULL)
	{
		if (object->isMarked)		// object marked, do not free
		{
			object->isMarked = false;			// reset the marking to 'white'
			previous = object;
			object = object->next;
		}
		else      // free the unreachable object
		{
			Obj* unreached = object;
			object = object->next;

			if (previous != NULL)	// link to previous object if previous not null
			{
				previous->next = object;
			}
			else             // if not set the next as the start of the list
			{
				vm.objects = object;	
			}

			freeObject(unreached);			// method that actually frees the object
		}
	}
}

void collectGarbage()
{
#ifdef DEBUG_LOG_GC
	printf("--Garbage Collection Begin\n");
	size_t before = vm.bytesAllocated;
#endif

	markRoots();			// function to start traversing the graph, from the root and marking them
	traceReferences();		// tracing each gray marked object

	// removing intern strings, BEFORE the sweep so the pointers can still access its memory
	// function defined in hahst.c
	tableRemoveWhite(&vm.strings);

	sweep();				// free all unreachable roots

	// adjust size of threshold
	vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
	printf("--Garbage Collection End\n");
	printf("	collected %ld bytes (from %ld to %ld) next at %ld\n",
		before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}


/*		end of garbage collection		 */



void freeObjects()			// free from VM
{
	Obj* object = vm.objects;
	// free from the whole list
	while (object != NULL)
	{
		Obj* next = object->next;
		freeObject(object);
		object = next;
	}

	free(vm.grayStack);			// free gray marked obj stack used for garbage collection
}