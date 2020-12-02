// the compiler, parses source code and outputs low-level series of binary instructions

#ifndef compiler_h
#define compiler_h

#include "object.h"
#include "virtualm.h"

bool compile(const char* source, Chunk* chunk);			// receives the source code(in charray) and the chunk itself

#endif