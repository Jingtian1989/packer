#ifndef _MY_MEM_H
#define _MY_MEM_H

#define ALLOC(nbytes) \
	mem_alloc((nbytes), __FILE__, __LINE__)

#define CALLOC(count, nbytes) \
	mem_calloc((count), (nbytes), __FILE__, __LINE__)

#define NEW(p) ((p) = ALLOC((long)sizeof(*p)))

#define NEW0(p) ((p) = CALLOC(1, (long)sizeof(*p)))

#define FREE(ptr) ((void)(mem_free(ptr, __FILE__, __LINE__)), ptr = 0)

extern void *mem_alloc(long nbytes, const char *file, int line);
extern void *mem_calloc(long count, long nbytes, const char *file, int line);
extern void  mem_free(void *ptr, const char *file, int line);

#endif
