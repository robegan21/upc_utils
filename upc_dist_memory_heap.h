//
//  upc_memory_heap.h
//  
//
//  Created by Rob Egan on 12/12/14.
//  Inspired by Evangelos Georganas
//
//

#ifndef _upc_dist_memory_heap_h
#define _upc_dist_memory_heap_h

#include <upc.h>
#include <assert.h>

#include "upc_utils.h"

#ifndef UPC_HEAP_BLOCK_SIZE
#define UPC_HEAP_BLOCK_SIZE 1
#endif

#ifndef UPC_HEAP_THREAD_BITS
#define UPC_HEAP_THREAD_BITS 24
#endif

#define UPC_HEAP_THREAD_MASK ( (UPC_INT64_T) ((UPC_INT64_T)(1<<24) - 1L) )
#define UPC_HEAP_OFFSET_MASK ( (UPC_INT64_T) !UPC_HEAP_OFFSET_MASK )

// The HeapType is the base unit of a heap
// The HeapAllocation datastructure is the first bytes of any allocated region
// HeapPos is a composite int64 with UPC_HEAP_THREAD_BITS (24) low bits set for the thread
// and 64-UPC_HEAP_THREAD_BITS (40) high bits set for the offset from the start of the Heap
// offset will always be non-zero as 0 byte offset will be the HeapAllocation memory position itself

typedef shared[] char * BytesPtr;
typedef char HeapType;
typedef HeapType *HeapTypePtr;
typedef shared[] HeapTypePtr SharedHeapTypePtr;
typedef HeapAllocation;
struct {
    // start is address of this HeapAllocation (should never be on the stack)
    UPC_INT64_T heapOffset; // in bytes. may be any valid int, heapOffset points to start of this allocation relative to the tail (first) HeapAllocation of a DistHeapHandle
    UPC_INT64_T size;   // in bytes (>= sizeof(HeapAllocation)
    UPC_INT64_T offset; // offset in bytes of last claimed HeapType element. always 0 < sizeof(HeapAllocation) <= offset < size
    UPC_INT64_T confirmed; // offset in bytes of last fenced write. always offset <= confirmed <= size.  In flight if confirmed != size
    HeapAllocation *next; // linked list of heaps
} HeapAllocation;
typedef shared[] HeapAllocation *SharedHeapAllocationPtr;

struct {
    // Not an offset, not a thread but a combination of both in 64 bits (1 word)
    // Use this in distributed data structures and get/set it atomically
    UPC_INT64_T heapPos;
} HeapPos;

#define GET_HEAP_THREAD( heapPos ) ( ( heapPos & UPC_HEAP_THREAD_MASK ) )
#define GET_HEAP_OFFSET( heapPos ) ( ( heapPos & UPC_HEAP_OFFSET_MASK ) >> UPC_HEAP_THREAD_BITS )
#define GET_HEAP_POS( thread, offset ) ( (UPC_INT64_T) ((((UPC_INT64_T)thread)&UPC_HEAP_THREAD_MASK) & ( (((UPC_INT64_T)offset)<<UPC_HEAP_THREAD_BITS) & UPC_HEAP_OFFSET_MASK)) )

struct {
    // All data is on a single thread
    UPC_INT64_T requestedIncrease; // used to signal insufficient space.  Threads much check periodically, if used
    SharedHeapAllocationPtr heapHead; // current heap head
    SharedHeapAllocationPtr heapTail; // never changes after initialization... all HeapPos are relative to this 
} DistHeapHandle;
typedef shared[UPC_HEAP_BLOCK_SIZE] DistHeapHandle *DistHeapHandlePtr; // used for global array 1 per THREAD

// Used to efficiently copy/access a heap with the first allocation (when only 1 allocation is made)
struct {
    DistHeapHandle handle;
    SharedHeapAllocationPtr headHeap;
    HeapAllocation headHeapCopy;
} CachedDistHeapData;
typedef CachedDistHeapData *CachedDistHeapDataPtr;
struct {
    DistHeapHandlePtr distHeap;
    CachedDistHeapDataPtr cachedDistHeap;
} CachedDistHeapHandle;
typedef CachedDistHeapHandle *CachedDistHeapHandlePtr;

CachedDistHeapHandlePtr createCachedDistHeap(DistHeapHandlePtr distHeap) {
    CachedDistHeapHandlePtr cachedDistHeap = (CachedDistHeapHandlePtr) malloc(sizeof(CachedDistHeapHandle) + THREADS*sizeof(CachedDistHeapData));
    if (cachedDistHeap == NULL) {
        LOG("Could not allocate memory for the cached distributed heap: %ld bytes\n", THREADS*sizeof(CachedDistHeapHandle));
        upc_global_exit(1);
    }
    cachedDistHeap->distHeap = distHeap;
    cachedDistHeap->cachedDistHeap = ((BytePtr) cachedDistHeap) + sizeof(CachedDistHeapHandle);
    for(int i = 0; i < THREADS; i++) {
        updateCachedDistHeap(cachedDistHeap, i);
    }
    return cachedDistHeap;
}

void destroyCachedDistHeap(CachedDistHeapHandlePtr cached) {
    cached->distHeap = NULL;
    cached->cachedDistHeap = NULL;
    free(cached);
}

void updateCachedDistHeap(CachedDistHeapHandlePtr cached, int thread) {
    CachedDistHeapDataPtr cachedThreadHandle = cached->cachedDistHeap + thread;
    cachedThreadHandle->handle = cached->distHeap[thread]; // copy remote to local
    cachedThreadHandle->headHeap = cachedThreadHandle->handle.headHeap;  // shared pointer
    cachedThreadHandle->headHeapCopy = *(cachedThreadHandle->handle.headHeap); // copy remote HeapAllocation to local
}

HeapPos getHeapPosFromDistHeapHandle(DistHeapHandlePtr heapPtr, SharedHeapTypePtr ptr) {
    HeapPos hp;
    UPC_INT64_T thread = upc_threadof(ptr);
    assert( upc_threadof( heapPtr[thread].heapTail ) == thread );
    UPC_INT64_T offset = (BytesPtr)ptr - (BytesPtr) heapPtr[thread].heapTail;
    hp.heapPos = GET_HEAP_POS(thread,offset);
    return hp;
}

HeapPos getHeapPosFromCachedDistHeapHandle(CachedDistHeapHandlePtr cachedDistHeapHandle, SharedHeapTypePtr ptr) {
    HeapPos hp;
    UPC_INT64_T thread = upc_threadof(ptr);
    SharedHeapAllocationPtr heapTail = cachedDistHeapHandle->distHeap[thread].heapTail;
    assert(cachedDistHeapHandle->distHeap[thread].heapTail == heapTail);
    assert( upc_threadof( heapTail ) == thread );
    UPC_INT64_T offset = (BytesPtr) ptr - (BytesPtr) heapTail;
    hp.heapPos = GET_HEAP_POS(thread,offset);
    return hp;
}
    

SharedHeapTypePtr getSharedPtrFromDistHeapHandle(DistHeapHandlePtr heapPtr, HeapPos heapPos) {
    UPC_INT64_T thread = GET_HEAP_THREAD( heapPos.heapPos );
    UPC_INT64_T offset = GET_HEAP_OFFSET( heapPos.heapPos );
    assert(thread >= 0);
    assert(thread < THREADS);
    SharedHeapAllocationPtr heapTail = heapPtr[thread].heapTail;
    assert( upc_threadof(heapTail) == thread );
    assert( upc_threadof(heapTail + offset) == thread );
    return (SharedHeapTypePtr) (((BytesPtr) heapTail) + offset);
}

SharedHeapTypePtr getSharedPtrFromCachedDistHeapHandle(CachedDistHeapHandlePtr cachedDistHeapHandle, HeapPos heapPos) {
    UPC_INT64_T thread = GET_HEAP_THREAD( heapPos.heapPos );
    UPC_INT64_T offset = GET_HEAP_OFFSET( heapPos.heapPos );
    assert(thread >= 0);
    assert(thread < THREADS);
    SharedHeapAllocationPtr heapTail = cachedDistHeapHandle->distHeap[thread].heapTail;
    assert( cachedDistHeapHandle->distHeap[thread].heapTail == heapTail);
    assert( upc_threadof(heapTail) == thread );
    assert( upc_threadof(heapTail + offset) == thread );
    return (SharedHeapTypePtr) (((BytesPtr) heapTail) + offset);
}

SharedHeapAllocationPtr constructHeapAllocation( UPC_INT64_T mySize, BytePtr origin ) {

    int extraBytes = sizeof(HeapAllocation) % sizeof(HeapType);
    extraBytes = extraBytes == 0 ? 0 : sizeof(HeapAllocation) - extraBytes2;
    int heapAllocSize = sizeof(HeapAllocation) + extraBytes + sizeof(HeapType) * mySize;
    SharedHeapAllocationPtr heapAlloc = (SharedHeapAllocationPtr) upc_alloc( heapAllocSize );
    if (heapAlloc == NULL) {
       LOG("Could not upc_alloc %ld bytes\n", heapAllocSize);
       upc_global_exit(1);
    }
    assert(upc_threadof(heapAlloc) == MYTHREAD);
    heapAlloc->heapOffset = origin == NULL ? 0 : origin - (BytesPtr) heapAlloc;
    heapAlloc->size = heapAllocSize;
    heapAlloc->offset = sizeof(HeapAllocation) + extraBytes;
    heapAlloc->confirmed = heapAlloc->offset;
    heapAlloc->next = NULL;
    upc_fence;
    return heapAlloc;
}

// collective. Creates a global array: one heap per thread with mySize elements of HeapType
DistHeapHandlePtr constructDistHeap(UPC_INT64_T mySize) {
    DistHeapHandlePtr distHandle = (DistHeapHandlePtr) upc_all_alloc(THREADS, sizeof(DistHeapHandle))
    if (distHandle == NULL) {
       LOG("Could not allocate %ld bytes in upc_all_alloc", sizeof(DistHeapHandle)*THREADS);
       upc_global_exit(1);
    } 
    assert(upc_threadof(distHandle+MYTHREAD) == MYTHREAD);
    distHandle[MYTHREAD]->requestedIncrease = 0;
    SharedHeapAllocationPtr heapAlloc = constructHeapAllocation(mySize, NULL);
    assert(upc_threadof(heapAlloc) == MYTHREAD);
    distHandle[MYTHREAD]->heapHead = heapAlloc;
    distHandle[MYTHREAD]->heapTail = heapAlloc;
    upc_fence;
    return distHandle;
}

void destroyHeapAlloc(SharedHeapAllocationPtr head) {
    assert(head != NULL);
    SharedHeapAllocationPtr next;
    while(head != NULL) {
       next = head->next;
       head->size = 0;
       head->next = NULL;
       upc_free(head);
       head = next;
    }
}

void destroyDistHeap(DistHeapHandlePtr distHeap) {
    assert(distHeap != NULL);
    SharedHeapAllocationPtr heapAlloc = distHeap[MYTHREAD].heapHead;
    assert(heapAlloc != NULL);
    destroyHeapAlloc(heapAlloc);
    distHeap[MYTHREAD].heapHead = NULL;
    distHeap[MYTHREAD].heapTail = NULL;
    upc_all_free(distHeap);
}

// returns NULL pointers and 0 count if unsuccessful
struct {
    SharedHeapTypePtr ptr;
    SharedHeapAllocationPtr heapAlloc;
    UPC_INT64_t count;
} AllocatedHeap;

AllocatedHeap tryAllocRange(DistHeapHandlePtr distHeap, UPC_INT64_T thread, UPC_INT64_T count) {
    assert(thread >= 0);
    assert(thread < THREADS);
    SharedHeapAllocationPtr heapAlloc = distHeap[thread]->heapHead;
    assert(heapAlloc != NULL);
    HeapAllocation localHeapAlloc = *heapAlloc;
    AllocatedHeap ret;
    ret.ptr = NULL;
    ret.heapAlloc = NULL;
    ret.count = 0;
    UPC_INT64_t requiredSize = count * sizeof(HeapType);
    if (localHeapAlloc.size <= localHeapAlloc.offset + requiredSize) { 
        UPC_INT64_T myOffset = UPC_ATOMIC_FADD_I64( &(heapAlloc->offset), requiredSize );
        if (myOffset + requiredSize <= localHeapAlloc.size) {
          // Success! return allocated information
          ret.ptr = ((BytePr) heapAlloc) + heapAlloc->heapOffset + myOffset;
          ret.heapAlloc = heapAlloc;
          ret.count = count;
        } else {
          // Failure! return no allocation
          UPC_INT64_T corrected = UPC_ATOMIC_FADD_I64( &(heapAlloc->offset), -requiredSize );
          if (distHeap->head == heapAlloc) {
              // signal for more HeapAllocations
              UPC_ATOMIC_CSWAP( &distHeap->requestedIncrease, 0, requiredSize);
          }
        }
    }
    checkMyHeap(distHeap);
    return ret;
}

// returns 0 if unsuccessful, count otherwise
UPC_INT64_T tryPutData(DistHeapHandlePtr distHeap, UPC_INT64_T thread, HeapType *data, UPC_INT64_T count) {
    AllocatedHeap allocated = tryAllocRange(distHeap, thread, count);;
    if (allocated.count == count) {
        assert(allocated.ptr != NULL);
        assert(upc_threadof(allocated.ptr) == thread);
        upc_memput(allocated.ptr, data, count*sizeof(HeapType));
        upc_fence;
        UPC_INT64_T confirmed = UPC_ATOMIC_FADD_I64( &(allocated.heapAlloc->confirmed), count*sizeof(HeapType) ); 
        assert(confirmed + count*sizeof(HeapType) <= allocated.heapAlloc->size);
        return count; 
    } else {
        // Failure!
        assert(allocated == 0);
        return 0;
    }
}

void checkMyHeap(DistHeapHandlePtr distHeap) {
    assert(upc_threadof(distHeap + MYTHREAD) == MYTHREAD);
    UPC_INT64_T requestedIncrease = distHeap[MYTHREAD].requestedIncrease;
    if (requestedIncrease != 0) {
        SharedHeapAllocationPtr lastHead = distHeap[MYTHREAD].head;
        assert(upc_threadof(lastHead) == MYTHREAD);
        UPC_INT64_T growSize = lastHead->size;
        if (distHeap[MYTHREAD].requestedIncrease > growSize) growSize = distHeap[MYTHREAD].requestedIncrease * 2;
        SharedHeapAllocationPtr heapAlloc = constructHeapAllocation(growSize, distHeap[MYTHREAD].heapTail); 
        if (heapAlloc == NULL) {
            LOG("Could not grow my heap by %ld bytes\n", growSize);
            upc_global_exit(1);
        }
        // Link old head to this new heapAlloc
        assert(heapAlloc->next == NULL);
        heapAlloc->next = distHeap[MYTHREAD].head;
        // Link this new to head
        assert(distHeap[MYTHREAD].head == lastHead);
        distHeap[MYTHREAD].head = heapAlloc;
        assert(requestedIncrease == distHeap[MYTHREAD].requestedIncrease);
        distHeap[MYTHREAD].requestedIncrease = 0;
        upc_fence;
    }
}

/* TODO implement tryAllocRangeCached and tryPutDataCached */

#endif
