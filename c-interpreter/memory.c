#include <stdlib.h>

#include "memory.h"

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