// for heap-allocated data, for large data types like strings, functinons, instances
#ifndef object_h
#define object_h

#include "common.h"
#include "value.h"


#define OBJ_TYPE(value)	(AS_OBJ(value)->type)		// extracts the tag

#define IS_STRING(value)	isObjType(value, OBJ_STRING)		// takes in raw Value, not raw Obj*

// macros to tell that it is safe when creating a tag, by returning the requested type
// take a Value that is expected to conatin a pointer to the heap, first returns pointer second the charray itself
#define AS_STRING(value)	((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)	(((ObjString*)AS_OBJ(value))->chars)		// get chars(char*) from ObjString pointer
	

typedef enum
{
	OBJ_STRING,
} ObjType;


struct Obj					// as no typedef is used, 'struct' itself will always havae to be typed
{
	ObjType type;
	struct Obj* next;		// linked list or intrusive list, to avoid memory leaks, obj itself as a node
							// traverse the list to find every object tha has been allocated on the heap
};

struct ObjString			// using struct inheritance
{
	Obj obj;
	int length;
	char* chars;
};

ObjString* takeString(char* chars, int length);			// create ObjString ptr from raw Cstring
ObjString* copyString(const char* chars, int length);	// note: const inside parameter means that parameter cannot be changed
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type)				// inline function, initialized in .h file
{
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif