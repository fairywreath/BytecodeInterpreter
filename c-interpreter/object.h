// for heap-allocated data, for large data types like strings, functinons, instances
// for functions and calls
#ifndef object_h
#define object_h

#include "common.h"
#include "chunk.h"
#include "hasht.h"
#include "value.h"


#define OBJ_TYPE(value)	(AS_OBJ(value)->type)		// extracts the tag

// macros for checking(bool) whether an object is a certain type
#define IS_BOUND_METHOD(value)	isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)		isObjType(value, OBJ_CLASS)
#define IS_FUNCTION(value)	isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)	isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)	isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)	isObjType(value, OBJ_STRING)		// takes in raw Value, not raw Obj*
#define IS_CLOSURE(value)	isObjType(value, OBJ_CLOSURE)

// macros to tell that it is safe when creating a tag, by returning the requested type
// take a Value that is expected to conatin a pointer to the heap, first returns pointer second the charray itself
// used to cast as an ObjType pointer, from a Value type
#define AS_BOUND_METHOD(value)	((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)		((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value)  ((ObjInstance*)AS_OBJ(value))
#define AS_CLOSURE(value)	((ObjClosure*)AS_OBJ(value))
#define AS_STRING(value)	((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)	(((ObjString*)AS_OBJ(value))->chars)		// get chars(char*) from ObjString pointer
#define AS_FUNCTION(value)	((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)	\
	(((ObjNative*)AS_OBJ(value))->function)

typedef enum
{
	OBJ_BOUND_METHOD,
	OBJ_INSTANCE,
	OBJ_CLASS,
	OBJ_CLOSURE,
	OBJ_FUNCTION,
	OBJ_NATIVE,
	OBJ_STRING,
	OBJ_UPVALUE
} ObjType;


struct Obj					// as no typedef is used, 'struct' itself will always havae to be typed
{
	ObjType type;
	struct Obj* next;		// linked list or intrusive list, to avoid memory leaks, obj itself as a node
							// traverse the list to find every object that has been allocated on the heap

	// for mark-sweep garbage collection
	bool isMarked;
};

// for functions and calls
typedef struct
{
	Obj obj;
	int arity;				// store number of parameters
	int upvalueCount;		// to track upValues
	Chunk chunk;			// to store the function information
	ObjString* name;
} ObjFunction;


typedef struct ObjUpvalue			// define ObjUpvalue here to use them inside the struct
{
	Obj obj;
	Value* location;			// pointer to value in the enclosing ObjClosure
	
	Value closed;		// to store closed upvalue
								
	// intrusive/linked list to track sorted openvalues
	// ordered by the stack slot they point to
	struct ObjUpvalue* next;
} ObjUpvalue;

// for closures
typedef struct
{
	// points to an ObjFunction and Obj header
	Obj obj;					// Obj header
	ObjFunction* function;
	
	// for upvalues
	ObjUpvalue** upvalues;		// array of upvalue pointers
	int upvalueCount;
} ObjClosure;


/*  NATIVE FUNCTIONS(file systems, user input etc.)
-> native functions reference a call to native C code insted of bytecode */
typedef Value(*NativeFn)(int argCount, Value* args);		// rename Value to NativeFn

typedef struct {
	Obj obj;
	NativeFn function;
} ObjNative;


struct ObjString			// using struct inheritance
{
	Obj obj;
	int length;
	char* chars;
	uint32_t hash;		// for hash table, for cache(temporary storage area); each ObjString has a hash code for itself
};


// class object type
typedef struct
{
	Obj obj;
	ObjString* name;			// not needed for uer's program, but helps the dev in debugging
	Table methods;				// hash table for storing methods
} ObjClass;

typedef struct
{
	Obj obj;			// inherits from object, the "object" tag
	ObjClass* kelas;	// pointer to class types
	Table fields;		// use a hash table to store fields
} ObjInstance;


// struct for class methods
typedef struct
{
	Obj obj;
	Value receiver;					// wraps receiver and function/method/closure together, receiver is the ObjInstance / lcass type
	ObjClosure* method;
} ObjBoundMethod;		


ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjInstance* newInstance(ObjClass* kelas);
ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);
ObjClosure* newClosure(ObjFunction* function);			// create closure from ObjFunction
ObjUpvalue* newUpvalue(Value* slot);

ObjString* takeString(char* chars, int length);			// create ObjString ptr from raw Cstring
ObjString* copyString(const char* chars, int length);	// note: const inside parameter means that parameter cannot be changed
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type)				// inline function, initialized in .h file
{
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif