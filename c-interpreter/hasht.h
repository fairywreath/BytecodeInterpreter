// hash table to store names, etc.
// in terms of the language's bytecode datatypes (eg. Value, objString)
// hash function is in object.c, applied to every string for cache
#ifndef hasht_h
#define hasht_h

#include "common.h"
#include "value.h"


typedef struct
{
	ObjString* key;			// use ObjString pointer as key
	Value value;			// the value/data type
} Entry;

// the table, an array of entries
typedef struct
{
	int count;
	int capacity;
	Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);		// put hash into table
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);			// copy everything from old table to new table
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);		// used in VM, for string interning

// removing string intern pointers
void tableRemoveWhite(Table* table);

// mark global variables, used in VM for garbage collection
void markTable(Table* table);

#endif