#ifndef FILE_MAP_H_
#define FILE_MAP_H_

#include "Buffer.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct {
    FILE *fh;
    char *addr;
    size_t myPos, myStart, myEnd, filesize;;
    char *filename;
    int myPartition, numPartitions;
    Buffer buf;
} _FileMap;
typedef _FileMap *FileMap;

size_t get_file_size(const char *fname);

FileMap initFileMap(const char *filename, const char *mode, int partition, int numPartitions);
void freeFileMap(FileMap *pfm);

//
void releaseMmapFileMap(FileMap fm);
int closeFileMap(FileMap fm);

void setMyPartitionFileMap(FileMap fm, int partition, int numPartitions);

size_t haveMoreFileMap(FileMap fm);

// acts on the mmap
char *getLineFileMap(FileMap fm);
size_t getPosFileMap(FileMap fm);

// acts on the FILE handle
char *fgetsFileMap(FileMap fm);
void rewindFileMap(FileMap fm);
size_t tellFileMap(FileMap fm);

#if defined (__cplusplus)
}
#endif

#endif
