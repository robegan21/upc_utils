#include <stdio.h>
#include <assert.h>

#include "Buffer.h"

#ifndef DIE
#define DIE(fmt, ...) do { fprintf(stderr, fmt, ##__VA_ARGS__); exit(1); } while(0)
#endif

// returns a new Buffer
Buffer initBuffer(size_t initSize) {
    if (initSize <= 0) initSize = 256;
    Buffer b = (_Buffer *) malloc(sizeof(_Buffer));
    if (b == NULL) { DIE("Could not allocate new Buffer!\n"); }
    b->buf = (char*) malloc(initSize);
    if (b->buf == NULL) { DIE("Could not allocate %ld bytes into Buffer!\n", initSize); }
    b->len = 0;
    b->size = initSize;
    assert(b->len < b->size);
    b->buf[b->len] = '\0';
    return b;
}
// returns the remaining size of the Buffer
size_t growBuffer(Buffer b, size_t appendSize) {
    assert(b != NULL);
    assert(b->size > 0);
    size_t requiredSize = b->len + appendSize + 1;
    if (requiredSize >= b->size) {
        while (requiredSize > b->size) {
            b->size *= 2;
        }
        assert(b->size >= requiredSize);
        b->buf = (char*) realloc(b->buf, b->size);
        if (b->buf == NULL)  { DIE("Could not reallocate %ld bytes into Buffer!", b->size); }
        b->buf[b->len] = '\0';
    }
    assert(b->len + appendSize < b->size);
    return b->size - b->len;
}
// sets the buffer len to 0, does not affect size
void resetBuffer(Buffer b) {
    assert(b != NULL);
    assert(b->buf != NULL);
    assert(b->size > 0);
    b->len = 0;
    b->buf[b->len] = '\0';
}
// destroys a Buffer
void freeBuffer(Buffer b) {
    if (b == NULL) return;
    free(b->buf);
    b->buf = NULL;
    b->len = b->size = 0;
    free(b);
}

size_t getLengthBuffer(Buffer b) {
    return b->len;
}
char * getStartBuffer(Buffer b) {
    return b->buf;
}
char *getEndBuffer(Buffer b) {
    return b->buf + b->len;
}

// writes to a buffer, reallocating, if necessary
// returns the length of the write
size_t printfBuffer(Buffer b, const char *fmt, ...)
{
    size_t requiredSize = -1;
    assert(b != NULL);
    {
        va_list args;

        va_start(args, fmt);
        requiredSize = vsnprintf(NULL, 0, fmt, args);
        va_end(args);

        assert(requiredSize > 0);
        growBuffer(b, requiredSize+1);
    }
    assert(b->size - b->len > 0);

    va_list args;
    va_start(args, fmt);
    size_t len = vsnprintf(getEndBuffer(b), b->size - b->len, fmt, args);
    va_end(args);

    assert(len == requiredSize);
    b->len += len;
    assert(b->len < b->size);
    return len;
}

char *fgetsBuffer(Buffer b, int size, FILE *stream)
{
    growBuffer(b, size);
    size_t oldLen = b->len;
    assert(oldLen < b->size);
    char *pos = fgets(getEndBuffer(b), size, stream);
    if (pos) {
        assert(pos == getEndBuffer(b));
        b->len += strlen(pos);
	assert(b->len < b->size);
    } else {
        b->buf[b->len] = '\0';
    }
    return pos;
}

void *memcpyBuffer(Buffer b, const void *src, size_t len)
{
    growBuffer(b, len+1);
    void * ret = memcpy(getEndBuffer(b), src, len);
    b->len += len;
    assert(b->size > b->len);
    return ret;
}

char *strcpyBuffer(Buffer b, const char *src)
{
    assert(src != NULL);
    size_t len = strlen(src);
    return strncpyBuffer(b, src, len);
}

char *strncpyBuffer(Buffer b, const char *src, size_t len)
{
    growBuffer(b, len + 1);
    char *ret = strcpy(getEndBuffer(b), src);
    b->len += len;
    assert(b->size > b->len);
    return ret;
}

int chompBuffer(Buffer b) {
    assert(b->len < b->size);
    if (b->size && b->buf[b->len-1] == '\n') {
        b->buf[ --(b->len) ] = '\0';
        return 1;
    } else {
        return 0;
    }
}

