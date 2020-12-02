#include <stdio.h>
#include <string.h>			// for memcmp

#include "memory.h"
#include "value.h"
#include "object.h"		// for macro AS_OBJ and ObjString*

void initValueArray(ValueArray* array)
{
	array->count = 0;
	array->capacity = 0;
	array->values = NULL;
}

void writeValueArray(ValueArray* array, Value value)
{
	if (array->capacity < array->count + 1)
	{
		int oldCapacity = array->capacity;
		array->capacity = GROW_CAPACITY(oldCapacity);
		array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
	}

	array->values[array->count] = value;
	array->count++;
}

void freeValueArray(ValueArray* array)
{
	FREE_ARRAY(Value, array->values, array->capacity);
	initValueArray(array);
}

// actual printing on the virtual machine is done here
void printValue(Value value)
{
	switch (value.type)
	{
	case VAL_BOOL:
		printf(AS_BOOL(value) ? "true" : "false"); break;
	case VAL_NULL:
		printf("null"); break;
	case VAL_NUMBER:
		printf("%g", AS_NUMBER(value)); break;
	case VAL_OBJ: printObject(value); break;			// print heap allocated value, from object.h
	}
}

// comparison function used in VM run()
// used in ALL types of data(num, string, bools)
bool valuesEqual(Value a, Value b)
{
	if (a.type != b.type) return false;				// if type is different return false

	switch (a.type)
	{
	case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
	case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
	case VAL_NULL: return true;				// true for all nulls
	case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);		// already interned, occupies the same address
	default:
		return false;		// unreachable
	}
}