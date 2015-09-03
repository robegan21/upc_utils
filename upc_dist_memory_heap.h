//
//  upc_memory_heap.h
//  
//
//  Created by Rob Egan on 12/12/14.
//  Inspired by Evangelos Georganas
//
//
/* The MIT License

  Copyright (c) 2015 Rob Egan

  Permission is hereby granted, free of charge, to any person obtaining a copy of 
  this software and associated documentation files (the "Software"), to deal in 
  the Software without restriction, including without limitation the rights to 
  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
  of the Software, and to permit persons to whom the Software is furnished to do 
  so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all 
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
  SOFTWARE.

*/

#ifndef _upc_dist_memory_heap_h
#define _upc_dist_memory_heap_h

#include <upc.h>
#include <assert.h>

#include "upc_utils.h"
#include "upc_nb_utils.h"

#ifndef UPC_HEAP_BLOCK_SIZE
#define UPC_HEAP_BLOCK_SIZE 1
#endif

#ifndef UPC_HEAP_THREAD_BITS
#define UPC_HEAP_THREAD_BITS 24
#endif

#define UPC_HEAP_THREAD_MASK ( (UPC_INT64_T) ((UPC_INT64_T)(1<<24) - 1L) )
#define UPC_HEAP_OFFSET_MASK ( (UPC_INT64_T) !UPC_HEAP_THREAD_MASK )

// The HeapType is the base unit of a heap
// The HeapAllocation datastructure is the first bytes of any allocated region
// HeapPos is a composite int64 with UPC_HEAP_THREAD_BITS (24) low bits set for the thread
// and 64-UPC_HEAP_THREAD_BITS (40) high bits set for the offset from the start of the Heap
// offset will always be non-zero as 0 byte offset will be the HeapAllocation memory position itself

typedef shared[] char * SharedBytesPtr;
typedef char HeapType;
typedef HeapType *HeapTypePtr;
typedef shared[] HeapType *SharedHeapTypePtr;

typedef struct HeapAllocation HeapAllocation;
typedef HeapAllocation *HeapAllocationPtr;
typedef shared[] HeapAllocation *SharedHeapAllocationPtr;
struct HeapAllocation {
    // start is address of this HeapAllocation (should never be on the stack)
    UPC_INT64_T heapOffset; // in bytes. may be any valid int, heapOffset points to start of this allocation relative to the tail (first) HeapAllocation of a DistHeapHandle
    UPC_INT64_T size;   // in bytes (>= sizeof(HeapAllocation)
    UPC_INT64_T offset; // offset in bytes of last claimed HeapType element. always 0 < sizeof(HeapAllocation) <= offset < size
    UPC_INT64_T confirmed; // offset in bytes of last fenced write. always offset <= confirmed <= size.  In flight if confirmed != size
    SharedHeapAllocationPtr next; // linked list of heaps
};

typedef struct HeapPos HeapPos;
struct HeapPos {
    // Not an offset, not a thread but a combination of both in 64 bits (1 word)
    // Use this in distributed data structures and get/set it atomically
    UPC_INT64_T heapPos;
};

#define GET_HEAP_THREAD( heapPos ) ( ( heapPos & UPC_HEAP_THREAD_MASK ) )
#define GET_HEAP_OFFSET( heapPos ) ( ( heapPos & UPC_HEAP_OFFSET_MASK ) >> UPC_HEAP_THREAD_BITS )
#define GET_HEAP_POS( thread, offset ) ( (UPC_INT64_T) ((((UPC_INT64_T)thread)&UPC_HEAP_THREAD_MASK) & ( (((UPC_INT64_T)offset)<<UPC_HEAP_THREAD_BITS) & UPC_HEAP_OFFSET_MASK)) )

struct DistHeapData {
    // All data is on a single thread
    SharedHeapAllocationPtr heapHead; // current heap head
    SharedHeapAllocationPtr heapTail; // never changes after initialization... all HeapPos are relative to this 
    UPC_INT64_T requestedIncrease; // used to signal insufficient space.  Threads much check periodically, if used
};
typedef struct DistHeapData DistHeapData;
typedef shared[UPC_HEAP_BLOCK_SIZE] DistHeapData *DistHeapDataPtr;

typedef struct DistHeapHandle DistHeapHandle;
struct DistHeapHandle {
    DistHeapDataPtr distHeapData; // used for global array 1 per THREAD
    NBBarrier nbb; // used to sync some operations
};
typedef DistHeapHandle *DistHeapHandlePtr;

// Used to efficiently copy/access a heap with the first allocation (when only 1 allocation is made)
typedef struct CachedDistHeapData CachedDistHeapData;
struct CachedDistHeapData {
    DistHeapData heapData;
    SharedHeapAllocationPtr heapHead;
    HeapAllocation heapHeadCopy;
};
typedef CachedDistHeapData *CachedDistHeapDataPtr;


typedef struct CachedDistHeapHandle CachedDistHeapHandle;
struct CachedDistHeapHandle {
    DistHeapHandlePtr distHeap;
    CachedDistHeapDataPtr cachedDistHeapData;
};
typedef CachedDistHeapHandle *CachedDistHeapHandlePtr;

void updateCachedDistHeap(CachedDistHeapHandlePtr cached, int thread) {
    CachedDistHeapDataPtr cachedThreadHandle = cached->cachedDistHeapData + thread;
    cachedThreadHandle->heapData = cached->distHeap->distHeapData[thread]; // copy remote to local
    cachedThreadHandle->heapHead = cachedThreadHandle->heapData.heapHead;  // shared pointer
    upc_memget(&(cachedThreadHandle->heapHeadCopy), cachedThreadHandle->heapHead, sizeof(HeapAllocation)); // copy remote HeapAllocation to local
}

CachedDistHeapHandlePtr constructCachedDistHeap(DistHeapHandlePtr distHeap) {
    CachedDistHeapHandlePtr cachedDistHeap = (CachedDistHeapHandlePtr) malloc(sizeof(CachedDistHeapHandle) + THREADS*sizeof(CachedDistHeapData));
    if (cachedDistHeap == NULL) {
        LOG("Could not allocate memory for the cached distributed heap: %lld bytes\n", THREADS*sizeof(CachedDistHeapHandle));
        upc_global_exit(1);
    }
    cachedDistHeap->distHeap = distHeap;
    cachedDistHeap->cachedDistHeapData = (CachedDistHeapDataPtr) ((char*) cachedDistHeap) + sizeof(CachedDistHeapHandle); // memory directly after CachedDistHeapHandle is the Data array
    for(int i = 0; i < THREADS; i++) {
        updateCachedDistHeap(cachedDistHeap, i);
    }
    return cachedDistHeap;
}

void destroyCachedDistHeap(CachedDistHeapHandlePtr *_cached) {
    assert(_cached != NULL);
    CachedDistHeapHandlePtr cached = *_cached;
    assert(cached != NULL);
    cached->distHeap = NULL; // no need to free (allocated before cache)
    cached->cachedDistHeapData = NULL; // no need to free (allocated as part of cacheHandle)
    free(cached);
    *_cached = NULL;
}

HeapPos getHeapPosFromDistHeapHandle(DistHeapHandlePtr heapPtr, SharedHeapTypePtr ptr) {
    HeapPos hp;
    UPC_INT64_T thread = upc_threadof(ptr);
    assert( upc_threadof( heapPtr->distHeapData[thread].heapTail ) == thread );
    UPC_INT64_T offset = (SharedBytesPtr)ptr - (SharedBytesPtr) heapPtr->distHeapData[thread].heapTail;
    hp.heapPos = GET_HEAP_POS(thread,offset);
    return hp;
}

HeapPos getHeapPosFromCachedDistHeapHandle(CachedDistHeapHandlePtr cachedDistHeapHandle, SharedHeapTypePtr ptr) {
    HeapPos hp;
    UPC_INT64_T thread = upc_threadof(ptr);
    SharedHeapAllocationPtr heapTail = cachedDistHeapHandle->distHeap->distHeapData[thread].heapTail;
    assert(cachedDistHeapHandle->distHeap->distHeapData[thread].heapTail == heapTail);
    assert( upc_threadof( heapTail ) == thread );
    UPC_INT64_T offset = (SharedBytesPtr) ptr - (SharedBytesPtr) heapTail;
    hp.heapPos = GET_HEAP_POS(thread,offset);
    return hp;
}
    

SharedHeapTypePtr getSharedPtrFromDistHeapHandle(DistHeapHandlePtr heapPtr, HeapPos heapPos) {
    UPC_INT64_T thread = GET_HEAP_THREAD( heapPos.heapPos );
    UPC_INT64_T offset = GET_HEAP_OFFSET( heapPos.heapPos );
    assert(thread >= 0);
    assert(thread < THREADS);
    SharedHeapAllocationPtr heapTail = heapPtr->distHeapData[thread].heapTail;
    assert( upc_threadof(heapTail) == thread );
    assert( upc_threadof(heapTail + offset) == thread );
    return (SharedHeapTypePtr) (((SharedBytesPtr) heapTail) + offset);
}

SharedHeapTypePtr getSharedPtrFromCachedDistHeapHandle(CachedDistHeapHandlePtr cachedDistHeapHandle, HeapPos heapPos) {
    UPC_INT64_T thread = GET_HEAP_THREAD( heapPos.heapPos );
    UPC_INT64_T offset = GET_HEAP_OFFSET( heapPos.heapPos );
    assert(thread >= 0);
    assert(thread < THREADS);
    SharedHeapAllocationPtr heapTail = cachedDistHeapHandle->distHeap->distHeapData[thread].heapTail;
    assert( cachedDistHeapHandle->distHeap->distHeapData[thread].heapTail == heapTail);
    assert( upc_threadof(heapTail) == thread );
    assert( upc_threadof(heapTail + offset) == thread );
    return (SharedHeapTypePtr) (((SharedBytesPtr) heapTail) + offset);
}

SharedHeapAllocationPtr constructHeapAllocation( UPC_INT64_T mySize, SharedHeapTypePtr origin ) {

    int extraBytes = sizeof(HeapAllocation) % sizeof(HeapType);
    extraBytes = extraBytes == 0 ? 0 : sizeof(HeapAllocation) - extraBytes;
    long long heapAllocSize = sizeof(HeapAllocation) + extraBytes + sizeof(HeapType) * mySize;
    SharedHeapAllocationPtr heapAlloc = (SharedHeapAllocationPtr) upc_alloc( heapAllocSize );
    if (heapAlloc == NULL) {
       LOG("Could not upc_alloc %lld bytes\n", heapAllocSize);
       upc_global_exit(1);
    }
    assert(upc_threadof(heapAlloc) == MYTHREAD);
    heapAlloc->heapOffset = origin == NULL ? 0 : ((SharedBytesPtr) origin) - ((SharedBytesPtr) heapAlloc);
    heapAlloc->size = heapAllocSize;
    heapAlloc->offset = sizeof(HeapAllocation) + extraBytes;
    heapAlloc->confirmed = heapAlloc->offset;
    heapAlloc->next = NULL;
    upc_fence;
    return heapAlloc;
}

// collective. Creates a global array: one heap per thread with mySize elements of HeapType
DistHeapHandlePtr constructDistHeap(UPC_INT64_T mySize) {
    DistHeapHandlePtr distHandle = (DistHeapHandlePtr) malloc(sizeof(DistHeapHandle));
    if (distHandle == NULL) {
       LOG("Could not allocate memory for DistHeapHandle");
       upc_global_exit(1);
       return NULL;
    }
    DistHeapDataPtr distHeapData = (DistHeapDataPtr) upc_all_alloc(THREADS, sizeof(DistHeapData));
    if (distHeapData == NULL) {
       LOG("Could not allocate %lld bytes in upc_all_alloc", sizeof(DistHeapData)*THREADS);
       upc_global_exit(1);
       return NULL;
    } 
    assert(upc_threadof(distHeapData+MYTHREAD) == MYTHREAD);
    distHeapData[MYTHREAD].requestedIncrease = 0;
    SharedHeapAllocationPtr heapAlloc = constructHeapAllocation(mySize, NULL);
    assert(upc_threadof(heapAlloc) == MYTHREAD);
    distHeapData[MYTHREAD].heapHead = heapAlloc;
    distHeapData[MYTHREAD].heapTail = heapAlloc;
    distHandle->distHeapData = distHeapData;
    distHandle->nbb = initNBBarrier();
    upc_fence;
    return distHandle;
}

void destroyHeapAlloc(SharedHeapAllocationPtr *_head) {
    assert(_head != NULL);
    SharedHeapAllocationPtr head = *_head;
    assert(head != NULL);

    SharedHeapAllocationPtr next;
    while(head != NULL) {
       next = head->next;
       head->size = 0;
       head->next = NULL;
       upc_free(head);
       head = next;
    }
    *_head = NULL;
}

void destroyDistHeap(DistHeapHandlePtr *_distHeap) {
    assert(_distHeap != NULL);
    DistHeapHandlePtr distHeap = *_distHeap;
    assert(distHeap != NULL);
    if (distHeap->nbb != NULL) { destroyNBBarrier( &(distHeap->nbb) ); }
    SharedHeapAllocationPtr heapAlloc = distHeap->distHeapData[MYTHREAD].heapHead;
    assert(heapAlloc != NULL);
    destroyHeapAlloc(&heapAlloc);
    distHeap->distHeapData[MYTHREAD].heapHead = NULL;
    distHeap->distHeapData[MYTHREAD].heapTail = NULL;
    upc_all_free(distHeap->distHeapData);
    *_distHeap = NULL;
}

// returns NULL pointers and 0 count if unsuccessful
typedef struct AllocatedHeap AllocatedHeap;
struct AllocatedHeap {
    SharedHeapTypePtr ptr;
    SharedHeapAllocationPtr heapAlloc;
    UPC_INT64_T count;
};

void checkMyHeap(DistHeapHandlePtr distHeap) {
    UPC_POLL;
    assert(upc_threadof(distHeap->distHeapData + MYTHREAD) == MYTHREAD);
    UPC_INT64_T requestedIncrease = distHeap->distHeapData[MYTHREAD].requestedIncrease;
    if (requestedIncrease != 0) {
        SharedHeapAllocationPtr lastHead = distHeap->distHeapData[MYTHREAD].heapHead;
        assert(upc_threadof(lastHead) == MYTHREAD);
        UPC_INT64_T growSize = lastHead->size;
        if (distHeap->distHeapData[MYTHREAD].requestedIncrease > growSize) growSize = distHeap->distHeapData[MYTHREAD].requestedIncrease * 2;
        SharedHeapAllocationPtr heapAlloc = constructHeapAllocation(growSize, (SharedHeapTypePtr) distHeap->distHeapData[MYTHREAD].heapTail);
        if (heapAlloc == NULL) {
            LOG("Could not grow my heap by %lld bytes\n", growSize);
            upc_global_exit(1);
        }
        // Link old head to this new heapAlloc
        assert(heapAlloc->next == NULL);
        heapAlloc->next = distHeap->distHeapData[MYTHREAD].heapHead;
        // Link this new to head
        assert(distHeap->distHeapData[MYTHREAD].heapHead == lastHead);
        distHeap->distHeapData[MYTHREAD].heapHead = heapAlloc;
        assert(requestedIncrease == distHeap->distHeapData[MYTHREAD].requestedIncrease);
        distHeap->distHeapData[MYTHREAD].requestedIncrease = 0;
        upc_fence;
    }
}

AllocatedHeap tryAllocRange(DistHeapHandlePtr distHeap, UPC_INT64_T thread, UPC_INT64_T count) {
    assert(thread >= 0);
    assert(thread < THREADS);
    assert(upc_threadof( &(distHeap->distHeapData[thread]) ) == thread);
    SharedHeapAllocationPtr heapAlloc = distHeap->distHeapData[thread].heapHead;
    assert(heapAlloc != NULL);
    assert(upc_threadof(heapAlloc) == thread);
    HeapAllocation localHeapAlloc = *heapAlloc;
    AllocatedHeap ret;
    ret.ptr = NULL;
    ret.heapAlloc = NULL;
    ret.count = 0;
    UPC_INT64_T requestedIncrease = count * sizeof(HeapType);
    if (localHeapAlloc.size >= localHeapAlloc.offset + requestedIncrease) { 
        UPC_INT64_T myOffset = UPC_ATOMIC_FADD_I64( &(heapAlloc->offset), requestedIncrease );
        if (myOffset + requestedIncrease <= localHeapAlloc.size) {
          // Success! return allocated information
          ret.ptr = ((SharedBytesPtr) heapAlloc) + heapAlloc->heapOffset + myOffset;
          ret.heapAlloc = heapAlloc;
          ret.count = count;
        } else {
          // Failure! return no allocation
          UPC_INT64_T corrected = UPC_ATOMIC_FADD_I64( &(heapAlloc->offset), - requestedIncrease );
          if (distHeap->distHeapData[thread].heapHead == heapAlloc) {
              // signal for more HeapAllocations
              UPC_ATOMIC_CSWAP_I64( &(distHeap->distHeapData[thread].requestedIncrease), 0, requestedIncrease);
          }
        }
    } else {
        // signal for more HeapAllocations
        UPC_ATOMIC_CSWAP_I64( &(distHeap->distHeapData[thread].requestedIncrease), 0, requestedIncrease);
    }

    checkMyHeap(distHeap);
    return ret;
}

// returns NULL if unsuccessful, pointer to the start of the put data of count HeapTypes if successful
SharedHeapTypePtr tryPutData(DistHeapHandlePtr distHeap, UPC_INT64_T thread, HeapType *data, UPC_INT64_T count) {
    AllocatedHeap allocated = tryAllocRange(distHeap, thread, count);
    if (allocated.count == count) {
        assert(allocated.ptr != NULL);
        assert(upc_threadof(allocated.ptr) == thread);
        assert(upc_threadof(allocated.ptr + count) == thread);
        upc_memput(allocated.ptr, data, count*sizeof(HeapType));
        upc_fence;
        UPC_INT64_T confirmed = UPC_ATOMIC_FADD_I64( &(allocated.heapAlloc->confirmed), count*sizeof(HeapType) ); 
        assert(confirmed + count*sizeof(HeapType) <= allocated.heapAlloc->size);
        return allocated.ptr; 
    } else {
        // Failure!
        assert(allocated.count == 0);
        return NULL;
    }
}

// explicit barrier that allows completed threads to contiue to grow their allocation until all
// threads have completed
void distHeapBarrier(DistHeapHandlePtr distHeap) {

    loop_until( checkMyHeap(distHeap) , tryNBBarrier(distHeap->nbb) == THREADS );
    upc_barrier; // do not reset until all threads have finished trying
    resetNBBarrier(distHeap->nbb);

}


/* TODO implement tryAllocRangeCached and tryPutDataCached */

#endif
