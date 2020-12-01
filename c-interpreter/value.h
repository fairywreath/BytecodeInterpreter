// storing constants/literals
// separate "constant data" region
/*	IMPORTANT
as in C, everything is just bytes. 2 things to be aware of:
1. How to represent the TYPE of value?
2. How to store the VALUE itself?

EVERYTHING IN BITS

we use a TAGGED UNION, a value containg a TYPE TAG, and the PAYLOd / ACTUAL VALUE
*/
#ifndef value_h
#define value_h

#include "common.h"

// type tags for the tagged union
typedef enum
{
	VAL_BOOL,
	VAL_NULL,
	VAL_NUMBER,
} ValueType;

/* IMPORTANT 
-> use C unions to OVERLAP in memory for the STRUCT
->  size of the union is its LARGEST FIELD
-> unions are like structs but they only allocate memory for the LARGEST FIELD
*/

typedef struct
{
	ValueType type;
	union	// the union itself, implemented here
	{
		bool boolean;
		double number;
	} as;			// can use . to represent this union
} Value;


// type comparisons
#define IS_BOOL(value)		((value).type == VAL_BOOL)
#define IS_NULL(value)		((value).type == VAL_NULL)
#define IS_NUMBER(value)	((value).type == VAL_NUMBER)


// from raw C to value struct, given a value type return a raw C value
// nicely used in printing
// also for comparisons -> use values ALREADY CONVERTED to Value struct union to raw C
#define AS_BOOL(value)		((value).as.boolean)
#define AS_NUMBER(value)	((value).as.number)	


// macros for conversions from code type to struct Value union type
// pass Value struct to the macro
/*	IMPORTANT: macro syntax
#define macroname(parameter) (returntype)
-> here, return a Value type. initializing it inside the macro
-> . means as
*/
#define BOOL_VAL(value)		((Value){VAL_BOOL, {.boolean = value}})		
#define NULL_VAL			((Value){VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value)	((Value){VAL_NUMBER, {.number = value}})




// the constant pool is array of values
typedef struct
{
	int capacity;
	int count;
	Value* values;
} ValueArray;


bool valuesEqual(Value a, Value b);			// comparison function used in the VM

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif