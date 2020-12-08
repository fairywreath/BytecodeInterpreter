// the compiler, parses source code and outputs low-level series of binary instructions

#ifndef compiler_h
#define compiler_h

#include "object.h"
#include "virtualm.h"

ObjFunction* compile(const char* source);			// receives the source code(in charray) and the chunk itself

// for garbage collection
void markCompilerRoots();

#endif