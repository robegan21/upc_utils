#ifndef UPC_NB_UTILS_H
#define UPC_NB_UTILS_H

#include "upc.h"
#include "upc_utils.h"
#include <assert.h>

typedef struct _NBBarrier *NBBarrier;
struct _NBBarrier {
  shared[1] UPC_INT64_T *marks;
  unsigned char *cachedMarks;
};

void resetNBBarrier(NBBarrier nbb) {
    assert(nbb != NULL);
    assert(nbb->marks != NULL);
    assert(upc_threadof( &(nbb->marks[MYTHREAD])) == MYTHREAD);
    nbb->marks[MYTHREAD] = 0;
    upc_fence;
    for(int i = 0; i < THREADS; i++) {
        nbb->cachedMarks[i] = 0;
    }
}

// initializes a NBBarrier
NBBarrier initNBBarrier() {
    NBBarrier nbb = (NBBarrier) malloc(sizeof(struct _NBBarrier) + sizeof(unsigned char) * THREADS);
    if (nbb == NULL) {
        LOG("Could not allocate a NBBarrier!\n");
        upc_global_exit(1);
    }
    nbb->marks = (shared[1] UPC_INT64_T *) upc_all_alloc(THREADS, sizeof(UPC_INT64_T));
    if (nbb->marks == NULL) {
        LOG("Could not allocate NBBarrier!\n");
        upc_global_exit(1);
    }
    nbb->cachedMarks = (unsigned char*) (((char*) nbb) + sizeof(struct _NBBarrier));
    resetNBBarrier(nbb);
    return nbb;
}

void destroyNBBarrier(NBBarrier *_nbb) {
    assert(_nbb != NULL);
    NBBarrier nbb = *_nbb;
    assert(nbb != NULL);
    assert(nbb->marks != NULL);
    assert(nbb->cachedMarks != NULL);
    upc_all_free(nbb->marks);
    nbb->marks = NULL;
    nbb->cachedMarks = NULL; // no need to free nbb->cachedMarks as it was allocated as part of NBBarrier itself
    free(nbb);
    *_nbb = NULL;
}

// returns THREADS when all threads have called tryNBBarrier at least once, <THREADS when incomplete
UPC_INT64_T tryNBBarrier(NBBarrier nbb) {
    assert(nbb != NULL);
    assert(nbb->marks != NULL);
    assert(upc_threadof( &(nbb->marks[MYTHREAD])) == MYTHREAD);

    UPC_INT64_T ret = 0;
    int fails = 0;
    // avoid load imbalances, testing new failed thread each iteration
    UPC_INT64_T offset = nbb->marks[MYTHREAD]++ + MYTHREAD;
    for(UPC_INT64_T i = offset; i < offset+THREADS; i++) {
        int testThread = i % THREADS;
        if ( nbb->cachedMarks[testThread] > 0 ) {
            ret++;
        } else if ( nbb->marks[testThread] > 0 ) {
            nbb->cachedMarks[testThread] = 1;
            ret++;
        } else {
            fails++;
        }
        if (fails > 0) break;
    }
    assert( fails > 0 || ret == THREADS);
    return ret;
} 



#endif
