#include <stdlib.h>

#include "memory.h"
#include "virtualm.h"

// A void pointer is a pointer that has no associated data type with it.
// A void pointer can hold address of any type and can be typcasted to any type.
void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
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


void freeObject(Obj* object)		// to handle different types
{
	switch (object->type)
	{
	case OBJ_STRING: 
	{
		ObjString* string = (ObjString*)object;
		FREE_ARRAY(char, string->chars, string->length + 1);
		FREE(ObjString, object);
		break;
	}
	}
}

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
}