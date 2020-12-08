#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "virtualm.h"

void initChunk(Chunk* chunk)
{
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;			// dynamic array starts off completely empty
	chunk->lines = NULL;		// to store current line of code
	initValueArray(&chunk->constants);		// initialize constant list
}

void writeChunk(Chunk* chunk, uint8_t byte, int line)
{
	if (chunk->capacity < chunk->count + 1)				// check if chunk is full
	{
		int oldCapacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(oldCapacity);	// get size of new capacity
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);	// reallocate memory and grow array
		chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
	}
		
	chunk->code[chunk->count] = byte;	// code is an array, [] is just the index number
	chunk->lines[chunk->count] = line;
	chunk->count++;
}



void freeChunk(Chunk* chunk)
{
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);		// chunk->code is the pointer to the array, capacity is the size
	FREE_ARRAY(int, chunk->lines, chunk->capacity);
	freeValueArray(&chunk->constants);
	initChunk(chunk);
}

int addConstant(Chunk* chunk, Value value)
{
	push(value);			// garbage collection
	writeValueArray(&chunk->constants, value);
	pop();					// garbage collection
	return chunk->constants.count - 1;			// return index of the newly added constant
}

