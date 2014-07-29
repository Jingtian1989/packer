#include <stdlib.h>
#include <stddef.h>
#include "mem.h"

void *mem_alloc(long nbytes, const char *file, int line)
{
	void *ptr;
	ptr = malloc(nbytes);
	return ptr;
}

void *mem_calloc(long count, long nbytes, const char *file, int line)
{
	void *ptr;
	ptr = calloc(count, nbytes);
	return ptr;
}

void mem_free(void *ptr, const char *file, int line)
{
	if (ptr)
		free(ptr);
}