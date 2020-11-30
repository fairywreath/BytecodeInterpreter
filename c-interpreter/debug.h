// methods for the programmer/mantainer, not for the user
#ifndef debug_h
#define debug_h

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);		// diassemble all the instructions in a chunk
int disassembleInstruction(Chunk* chunk, int offset);		// diassembles a single instruction, offset being the index of instructio in the array


#endif