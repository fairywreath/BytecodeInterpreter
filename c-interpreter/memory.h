#ifndef memory_h
#define memory_h

#include "common.h"

// C macros
// calculates a new capacity based on a given current capacity, it should SCALE based on the old one
// this one grows by * 2
#define GROW_CAPACITY(capacity) \
	((capacity) < 8 ? 8 : (capacity) * 2)		// capacity becomes 8 for the first time(starts from 0), later it multiplies by 2

// macro to grow array
// make own reallocate function
// basically declare our return type here with (type*)
#define GROW_ARRAY(type, pointer, oldCount, newCount)	\
	 (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

// no (type*) because function does not return a type
// 0 is the new capacity
#define FREE_ARRAY(type, pointer, oldCount)	\
	reallocate(pointer, sizeof(type) * (oldCount), 0)	\

/* quick note on C memory alloc 
	sizeof(type) * count is a common parameter
	type being the data type (int, struct, etc)
	count being the size
*/

// function to reallocate arrays dynamically 
// all operations from allocation, freeing, chaning the size of existing ones
// void* mean it first accepts a data type
void* reallocate(void* pointer, size_t oldSize, size_t newSize);
	

#endif