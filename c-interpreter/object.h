// for heap-allocated data, for large data types like strings, functinons, instances
// for functions and calls
#ifndef object_h
#define object_h

#include "common.h"
#include "chunk.h"
#include "value.h"


#define OBJ_TYPE(value)	(AS_OBJ(value)->type)		// extracts the tag

#define IS_FUNCTION(value)	isObjType(value, OBJ_FUNCTION)
#define IS_STRING(value)	isObjType(value, OBJ_STRING)		// takes in raw Value, not raw Obj*

// macros to tell that it is safe when creating a tag, by returning the requested type
// take a Value that is expected to conatin a pointer to the heap, first returns pointer second the charray itself
#define AS_STRING(value)	((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)	(((ObjString*)AS_OBJ(value))->chars)		// get chars(char*) from ObjString pointer
#define AS_FUNCTION(value)	((ObjFunction*)AS_OBJ(value))

typedef enum
{
	OBJ_FUNCTION,
	OBJ_STRING,
} ObjType;


struct Obj					// as no typedef is used, 'struct' itself will always havae to be typed
{
	ObjType type;
	struct Obj* next;		// linked list or intrusive list, to avoid memory leaks, obj itself as a node
							// traverse the list to find every object that has been allocated on the heap
};

// for functions and calls
typedef struct
{
	Obj obj;
	int arity;				// store number of parameters
	Chunk chunk;			// to store the function information
	ObjString* name;
} ObjFunction;


struct ObjString			// using struct inheritance
{
	Obj obj;
	int length;
	char* chars;
	uint32_t hash;		// for hash table, for cache(temporary storage area); each ObjString has a hash code for itself
};

ObjFunction* newFunction();
ObjString* takeString(char* chars, int length);			// create ObjString ptr from raw Cstring
ObjString* copyString(const char* chars, int length);	// note: const inside parameter means that parameter cannot be changed
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type)				// inline function, initialized in .h file
{
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif