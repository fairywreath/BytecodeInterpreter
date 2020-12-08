#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "hasht.h"
#include "value.h"

// the hash table can only be 75% full
#define TABLE_MAX_LOAD 0.75

void initTable(Table* table)
{
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void freeTable(Table* table)
{
	FREE_ARRAY(Entry, table->entries, table->capacity);
	initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key)
{
	uint32_t index = key->hash % capacity;		// use modulo to map the key's hash to the code index
	Entry* tombstone = NULL;

	for (;;)
	{
		Entry* entry = &entries[index];		// index is 'inserted' here
		
		if (entry->key == NULL)
		{
			if (IS_NULL(entry->value))
			{	
				return tombstone != NULL ? tombstone : entry;		// empty entry
			}
			else {
				if (tombstone == NULL) tombstone = entry;		// can return tombstone bucket as empty and reuse it
			}
		}
		if (entry->key == key)		// compare them in MEMORY
		{
			return entry;
		}
		

		index = (index + 1) % capacity;		
	}
}

bool tableGet(Table* table, ObjString* key, Value* value)
{
	if (table->count == 0) return false;

	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	*value = entry->value;			// asign the value parameter the entry value
	return true;
}


static void adjustCapacity(Table* table, int capacity)
{
	Entry* entries = ALLOCATE(Entry, capacity);			// create a bucket with capacity entries, new array
	for (int i = 0; i < capacity; i++)		// initialize every element			
	{
		entries[i].key = NULL;				
		entries[i].value = NULL_VAL;
	}

	table->count = 0;		// do not copy tombstones over when growing
	// NOTE: entries may end up in different buckets
	// with the same hash as it is divided by the modulo; loop below recalculates everything
	for (int i = 0; i < table->capacity; i++)	// travers through old array
	{
		Entry* entry = &table->entries[i];
		if (entry->key == NULL) continue;		

		// insert into new array
		Entry* dest = findEntry(entries, capacity, entry->key);			// pass in new array
		dest->key = entry->key;					// match old array to new array
		dest->value = entry->value;
		table->count++;			// recound the number of entries
	}

	FREE_ARRAY(Entry, table->entries, table->capacity);
	table->entries = entries;
	table->capacity = capacity;
}

// inserting into the table, return false if collision
bool tableSet(Table* table, ObjString* key, Value value)
{
	// make sure array is big enough
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
	{
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capacity);
	}


	Entry* entry = findEntry(table->entries, table->capacity, key);

	bool isNewKey = entry->key == NULL;
	if (isNewKey && IS_NULL(entry->value)) table->count++;		// IS_NULL for tombstones; treat them as full objects

	entry->key = key;
	entry->value = value;

	return isNewKey;
}


bool tableDelete(Table* table, ObjString* key)
{
	if (table->count == 0) return false;

	// find entry
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	// place tombstone
	entry->key = NULL;
	entry->value = BOOL_VAL(true);		//BOOL_VAL(true) as the tombstone

	return true;
}

void tableAddAll(Table* from, Table* to)
{
	for (int i = 0; i < from->capacity; i++)
	{
		Entry* entry = &from->entries[i];
		if (entry->key != NULL)
		{
			tableSet(to, entry->key, entry->value);
		}
	}
}

// used in VM to find the string
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash)		// pass in raw character array 
{
	
	if (table->count == 0) return NULL;

	uint32_t index = hash % table->capacity;	// get index
	
	for (;;)
	{
		Entry* entry = &table->entries[index];		// get entry pointer
		if (entry->key == NULL)
		{
			// stop if found empty non-tombstone entry
			if (IS_NULL(entry->value)) return NULL;		// return null if not tombstone(tombstone value is BOOL_VAL(true))
		}
		else if (entry->key->length == length && entry->key->hash == hash
			&& memcmp(entry->key->chars, chars, length) == 0)
		{
			return entry->key;		// found the entry
		}

		index = (index + 1) % table->capacity;
	}
}

// removing unreachable pointers, used to remove string interns in garbage collection
void tableRemoveWhite(Table* table)
{
	for (int i = 0; i < table->capacity; i++)
	{
		Entry* entry = &table->entries[i];
		if (entry->key != NULL && !entry->key->obj.isMarked)		// remove not marked (string) object pointers
		{
			tableDelete(table, entry->key);
		}
	}
}


// mark global variables, used in VM for garbage collection
void markTable(Table* table)
{
	for (int i = 0; i < table->capacity; i++)
	{
		Entry* entry = &table->entries[i];
		// need to mark both the STRING KEYS and the actual value/obj itself
		markObject((Obj*)entry->key);			// mark the string key(ObjString type)
		markValue(entry->value);				// mark the actual avlue
	}
}