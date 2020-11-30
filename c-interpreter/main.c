#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "virtualm.h"

// for REPL, the print eval read loop 
static void repl()
{
	char line[1024];		// char array to hold everything, with a length limit
	for (;;)
	{
		printf(">> ");

		/* info on fgets
		- fgets is like cin or getline cin 
		- basically get a line everytime
		- parameters: fgets(char array, char size, filestream). In this case the file stream is stdin, the current keyboard or from the standard input
		- char array is a pointer to where the string will be copied
		- use if so that if line overloads, we go to the next line
		*/
		if (!fgets(line, sizeof(line), stdin))
		{
			printf("\n");
			break;
		}

		interpret(line);
	}
}

// get raw source code from file
static char* readFile(const char* path)
{
	/*	Reading files in C
	FILE* file = fopen(const char *file_name, const char *mode_of_operation
	r(read) = searches file, and sets up a pointer to the first character. If not found returns null
	w(write)
	a(read, set to last pointer)
	rb(special read to open non-text files, a binary file)
	*/
	FILE* file = fopen(path, "rb");
	if (file == NULL)			// if file does not exist or user does not have access
	{
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}

	// fseek - move file pointer to a specific position, offset is number of byte to offset form POSITION(last parameter)
	// int fseek(file pointer, long int offset, int position)
	// SEEK_END, SEEK_SET(start), SEEK_CUR
	// basically set pointer to end of file
	// 0l = 0 long
	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);		// ftell is used to find position of file pointer, used to denote size
	// top two lines used to get file size
	rewind(file);			// sets file pointer to the beginning of the file

	char* buffer = (char*)malloc(fileSize + 1);					// allocate a char*(string) to the size of the file
	if (buffer == NULL)
	{
		fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
		exit(74);
	}

	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);			// read the file
	/* notes on fread in C
	size_t fread(void * buffer, size_t size, size_t count, FILE * stream)
	buffer =  a pointer to the block of memoery with a least (size*count) byte
	size = size of the result type
	count = number of elements/ file size
	*/

	if (bytesRead < fileSize)			// if read size less than file size
	{
		fprintf(stderr, "Could not read \"%s\".\n", path);
		exit(74);
	}

	buffer[bytesRead] = '\0';		// mark the last character as '\0', the end of file symbol

	fclose(file);
	return buffer;
}

// function for loading scripts
static void runFile(const char* path)
{
	char* source = readFile(path);						// get raw source code from the file
	InterpretResult result = interpret(source);			// get enum type result from VM
	free(source);	// free the source code

	if (result == INTERPRET_COMPILE_ERROR) exit(51);
	if (result == INTERPRET_RUNTIME_ERROR) exit(61);
}


int main(int argc, const char* argv[])		// used in the command line, argc being the amount of arguments and argv the array
{
	initVM();
	// the FIRST argument will always be the name of the executable being run(e.g node, python in terminal)

	if (argc == 1)		// if number of argument is one, run the repl 
	{
		repl();
	}
	else if (argc == 2)	// if number of arguments is two, the second one being the file, run the second file
	{
		runFile(argv[1]);
	}
	else
	{
		fprintf(stderr, "Usage: cfei [path]\n");	// fprintf; print on file but not on console, first argument being the file pointer
													// in this case it prints STANDARD ERROR
		exit(64);
	}

	freeVM();
	return 0;
}