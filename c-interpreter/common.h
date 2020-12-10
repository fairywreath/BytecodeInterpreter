// VM
#ifndef common_h
#define common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// local variables array in compiler, max of 1 byte is 255
#define UINT8_COUNT (UINT8_MAX + 1)


// track the compiler
#define DEBUG_PRINT_CODE

// execution tracing of the VM
#define DEBUG_TRACE_EXECUTION


// diagnostic tools for garbage collector	
// 'stress' mode; if this is on, GC runs as often as it possibly can
//#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC


# endif