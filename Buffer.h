#ifndef _BUFFER_H_
#define _BUFFER_H_


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct {
        char *buf;
        size_t len, size;
} _Buffer;
typedef _Buffer *Buffer;
Buffer initBuffer(size_t initSize); 
size_t growBuffer(Buffer b, size_t appendSize); 
void resetBuffer(Buffer b);
void freeBuffer(Buffer b);
size_t getLengthBuffer(Buffer b);
char * getStartBuffer(Buffer b);
char *getEndBuffer(Buffer b);

size_t printfBuffer(Buffer b, const char *fmt, ...);
char *fgetsBuffer(Buffer b, int size, FILE *stream);
void *memcpyBuffer(Buffer b, const void *src, size_t n);
char *strcpyBuffer(Buffer b, const char *src);
char *strncpyBuffer(Buffer b, const char *src, size_t n);
int chompBuffer(Buffer b);

#if defined (__cplusplus)
}
#endif


#endif
