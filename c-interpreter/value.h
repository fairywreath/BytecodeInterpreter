// storing constants/literals, specifically double values
// separate "constant data" region
#ifndef value_h
#define value_h

#include "common.h"

typedef double Value;		// storing double precision floating point numbers

// the constant pool is array of values
typedef struct
{
	int capacity;
	int count;
	Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif