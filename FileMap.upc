#include <upc.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/stat.h>

#include "FileMap.h"

#ifndef DIE
#define DIE(fmt,...)                                                                                                    \
    do {                                                                \
        fprintf(stderr, "Thread %d, DIE [%s:%d]: " fmt,             \
                MYTHREAD, __FILE__, __LINE__, ##__VA_ARGS__);           \
        upc_global_exit(1);                                                   \
    } while (0)
#endif

size_t get_file_size(const char *fname)
{
    struct stat s;
    if (stat(fname, &s) != 0)
        DIE("could not stat %s: %s\n", fname, strerror(errno));
    return s.st_size;
}

FileMap initFileMap(const char *filename, const char *mode, int myPartition, int numPartitions) {
    FileMap fm = (FileMap) calloc(sizeof(_FileMap), 1);
    if (!fm) DIE("Could not calloc a FileMap");
    fm->filesize = get_file_size(filename);
    fm->fh = fopen(filename, mode);
    if (!fm->fh) DIE ("Could not open %s as '%s'!", filename, mode);
    fm->buf = initBuffer(256);
    fm->filename = strdup(filename);
    fm->myPartition = 0;
    fm->numPartitions = 0;
    if (numPartitions > 1) {
        setMyPartitionFileMap(fm, myPartition, numPartitions);
    } else {
        fm->myStart = 0;
        fm->myEnd = fm->filesize;
    }
#ifndef NO_MMAP
    fm->blockOffset = fm->myStart % 4096;
    size_t size = fm->myEnd - fm->myStart;
    if (size > 0) {
        fm->addr = mmap(NULL, size + fm->blockOffset, PROT_READ, MAP_FILE | MAP_SHARED, fileno(fm->fh), fm->myStart - fm->blockOffset);
    } else {
        fm->addr = NULL;
    }
#endif
    return fm;
}

void freeFileMap(FileMap *pfm) {
    FileMap fm = *pfm;
    releaseMmapFileMap(fm);
    closeFileMap(fm);

    free(fm->filename);
    fm->filename = NULL;
    freeBuffer(fm->buf);
    fm->buf = NULL;
    free(fm);
    *pfm = NULL;
}

void releaseMmapFileMap(FileMap fm) {
    if (fm->addr) munmap(fm->addr, fm->myEnd - fm->myStart + fm->blockOffset);
    fm->addr = NULL;
    fm->myStart = 0;
    fm->myEnd = 0;
}

int closeFileMap(FileMap fm) {
    int ret = fclose(fm->fh);
    fm->fh = NULL;
    return ret;
}

void setMyPartitionFileMap(FileMap fm, int myPartition, int numPartitions) {
    assert(fm != NULL);
    assert(fm->filesize > 0);
    if (myPartition != fm->myPartition || numPartitions != fm->numPartitions) {
        size_t blockSize = (fm->filesize + numPartitions - 1) / numPartitions;
        fm->myStart = blockSize * myPartition;
        fm->myEnd = fm->myStart + blockSize;
        assert(fm->myStart <= fm->filesize);
        if (fm->myStart >= fm->filesize) {
            fm->myStart = fm->filesize;
        }
        if (fm->myEnd >= fm->filesize) {
            fm->myEnd = fm->filesize;
        }
        if (myPartition < numPartitions-1 && fm->myEnd < fm->filesize) {
            seekFileMap(fm, fm->myEnd);
            // read next line
            fgetsFileMap(fm);
            fm->myEnd = fm->myPos - 1;
        }

        if (myPartition && fm->myStart < fm->myEnd) {
            seekFileMap(fm, fm->myStart);
            // read next line
            fgetsFileMap(fm);
            fm->myStart = fm->myPos;
        } else if (myPartition == 0) {
            assert(fm->myStart == 0);
        } else {
            fm->myStart = fm->myPos = fm->myEnd;
        }
        fm->myPartition = myPartition;
        fm->numPartitions = numPartitions;
    }
    seekFileMap(fm, fm->myStart);
#ifndef NO_MMAP
    size_t offset = fm->myStart % 4096;
    //if(fm->addr) madvise(fm->addr + fm->myStart - offset, fm->myEnd - fm->myStart + offset, MADV_WILLNEED);
#endif
}

size_t haveMoreFileMap(FileMap fm) {
   if (fm->myPos > fm->myEnd) {
       // reached last line, update myEnd to the current position
       fm->myEnd = fm->myPos;
   }
   return (fm->myEnd - fm->myPos);
}

// acts on the mmap
char *getLineFileMap(FileMap fm) { 
#ifdef NO_MMAP
    return fgetsFileMap(fm);
#endif
    resetBuffer(fm->buf);
    if (fm->myPos >= fm->myEnd) return NULL;
    char *start = fm->addr + (fm->myPos - fm->myStart + fm->blockOffset);
    size_t maxLen = fm->myEnd - fm->myPos;
    char *end = (char*)memchr(start, '\n', maxLen);
    if (!end) 
        return NULL;
    size_t len = end - start + 1;
    assert(len <= maxLen);
    char *buf = (char*) memcpyBuffer(fm->buf, start, len);
    buf[len] = 0;
    fm->myPos += len;
    return buf;
}

size_t getPosFileMap(FileMap fm) {
    return fm->myPos;
}

// acts on the FILE handle
char *fgetsFileMap(FileMap fm) {
    resetBuffer(fm->buf);
    do {
        char *line = fgetsBuffer(fm->buf, 4095, fm->fh);
        if (line==NULL || feof(fm->fh) != 0) break;
    } while (*(getStartBuffer(fm->buf) + getLengthBuffer(fm->buf) - 1) != '\n');
    fm->myPos = ftell(fm->fh);
    return getStartBuffer(fm->buf);
}

void rewindFileMap(FileMap fm) {
    setMyPartitionFileMap(fm, fm->myPartition, fm->numPartitions);
    seekFileMap(fm, fm->myStart);
}
size_t tellFileMap(FileMap fm) {
    return fm->myPos;
}

void seekFileMap(FileMap fm, size_t pos) {
    if (pos > fm->filesize) DIE("Invalid seek to %ld in %s with %ld bytes\n", pos, fm->filesize, fm->filename);
    fm->myPos = pos;
    fseek(fm->fh, fm->myPos, SEEK_SET);
}

