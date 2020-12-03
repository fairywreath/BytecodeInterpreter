// VM
#ifndef common_h
#define common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// track the compiler
#define DEBUG_PRINT_CODE

// execution tracing of the VM
#define DEBUG_TRACE_EXECUTION

// local variables array in compiler, max of 1 byte is 255
#define UINT8_COUNT (UINT8_MAX + 1)

# endif