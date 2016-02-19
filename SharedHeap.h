#ifndef SHARED_HEAP_H_
#define SHARED_HEAP_H_

#include <assert.h>

#include "CommonParallel.h"

#ifdef __UPC__
  #include <upc.h>
#elif defined _OPENMP
  #include <omp.h>
#elif defined USE_MPI
  #include <mpi.h>
#else
  #warn "No parallelism has been chosen at compile time... did you want OpenMP (cc -fopenmp), MPI (mpicc -DUSE_MPI) or UPC (upcc)?"
#endif

#if defined (__cplusplus)
extern "C" {
#endif

#ifndef MEM_ALIGN_SIZE
#define MEM_ALIGN_SIZE 8
#endif
// round up to nearest word
#define ALIGNED_MEM_SIZE(s) ( ((size_t) s + (MEM_ALIGN_SIZE - 1)) & ~((size_t) (MEM_ALIGN_SIZE - 1)) )

#ifdef __UPC__

  // UPC 
  #include <upc.h>
  #include "upc_compatiblity.h"

  #define ATOMIC_FETCHADD UPC_ATOMIC_FADD_I64
  #define ATOMIC_CSWAP UPC_ATOMIC_CSWAP_I64

  typedef struct {
    size_t size, idx;
    // data of length size is assumed immediately after this data structure
  } _SharedHeap;
  typedef shared _SharedHeap *SharedHeap;
  typedef shared char * SharedPtr;

  static SharedPtr __alloc_from_SharedHeap(SharedHeap sharedHeap, size_t size) {
    size = ALIGNED_MEM_SIZE(size);
    size_t idx = ATOMIC_FETCHADD(&(sharedHeap->idx), size); \
    if (idx + size > sharedHeap->size) DIE("Thread %d: attempt to allocate past size of SharedHeap (%lld of %lld)\n", (long long idx), (long long) sharedHeap->size);
    SharedPtr data = (SharedPtr) sharedHeap;
    return data + idx + ALIGNED_MEM_SIZE( sizeof(_SharedHeap) );
  }

  /* only MYTHREAD == rank will alloate the blocks of memory */
  #define INIT_SHARED_HEAP(sharedHeap, blocks, bytes, rank) \
    SharedHeap sharedHeap = NULL; \
    if (MYTHREAD == rank) { \
      size_t align_bytes = ALIGNED_MEM_SIZE(bytes); \
      sharedHeap = (SharedHeap) upc_alloc(blocks * align_bytes + ALIGNED_MEM_SIZE( sizeof(_SharedHeap) ) ); \
      if (sharedHeap == NULL) DIE("Thread %d: could not allocate %ld bytes for SharedHeap\n", MYTHREAD, blocks * align_bytes); \
      sharedHeap->size = blocks * align_bytes; \
      sharedHeap->idx = 0; \
      upc_fence; \
    } while (0)

  #define FREE_SHARED_HEAP(sharedHeap) { upc_free(sharedHeap); sharedHeap = NULL; } while(0)
  #define ALLOC_FROM_SHARED_HEAP(sharedHeap, type, varname, count) shared type *varname = (shared type *) __alloc_from_SharedHeap(sharedHeap, sizeof(type) * count)
  #define SHARED_MEMGET(dst, src, size) upc_memget(dst, src, size)
  #define SHARED_MEMPUT(dst, src, size) upc_memput(dst, src, size) 

#else // NOT UPC

  #ifdef MPI_VERSION
    // // MPI, ensure mpi.h is loaded
    #include <mpi.h>

    struct {
      size_t size, idx;
      MPI_Win win;
      int rank;
      // data is assumed to be immediately following this data structure
    } _SharedHeap;
    typedef _SharedHeap *SharedHeap;
    struct {
      SharedHeap heap;
      size_t offset;
    } SharedPtr;

    static size_t __shared_atomic_fetchadd(SharedPtr ptr, size_t val) {
      size_t oldVal;
      CHECK_MPI( MPI_Win_lock( MPI_LOCK_EXCLUSIVE, ptr.heap->rank, 0, ptr.heap->win) );
      CHECK_MPI( MPI_Fetch_and_op( &val, &oldVal, MPI_LONG_LONG_INT, ptr.heap->rank, ptr.offset, MPI_SUM, ptr.heap->win ) );
      CHECK_MPI( MPI_Win_unlock( ptr.heap->rank, ptr.heap->win ) );
      return oldVal;  
    }
    #define ATOMIC_FETCHADD __shared_atomic_fetchadd
    #define ATOMIC_CSWAP TODO
    static SharedPtr __alloc_from_SharedHeap(SharedHeap sharedHeap, size_t size) {
      assert(sharedHeap);
      size = ALIGNED_MEM_SIZE(size);
      SharedPtr ptr;
      ptr.heap = sharedHeap;
      ptr.offset = ((char *) &(sharedHeap->idx)) - ((char *) sharedHeap);
      ptr.offset = ATOMIC_FETCHADD(ptr, size);
      if (ptr.offset + size > sharedHeap->size) DIE("Could not allocate %ld bytes from SharedHeap (on thread %d)\n", size, sharedHeap->rank);
      return ptr;
    }
    static void __shared_memput(SharedPtr dst, char *src, size_t size) {
      CHECK_MPI( MPI_Put(src, size, MPI_BYTE, dst.heap->rank, dst.offset, size, MPI_BYTE, dst.heap->win) );
      CHECK_MPI( MPI_Win_flush( dst.heap->rank, dst.heap->win ) );
    } 
    static void __shared_memget(char *dst, SharedPtr src, size_t size) {
      CHECK_MPI( MPI_Get(dst, size, MPI_BYTE, dst.heap->rank, dst.offset, size, MPI_BYTE, dst.heap->win) );
    } 

    /* only MYTHREAD == rank will allocate the blocks of memory */ 
    #define INIT_SHARED_HEAP(sharedHeap, blocks, bytes, rank) \
      SharedHeap sharedHeap; \
      { \
        size_t align_bytes = blocks * ALIGNED_MEM_SIZE(bytes); \
        size_t alloc_bytes = (MYTHREAD == rank ? align_bytes : 0) + ALIGNED_MEM_SIZE( sizeof(_SharedHeap) ); \
        CHECK_MPI( MPI_Alloc_mem(alloc_bytes, MPI_INFO_NULL, sharedHeap) ); \
        if (sharedHeap == NULL) DIE("Thread %d: could not allocate %ld bytes for SharedHeap\n", MYTHREAD, blocks * align_bytes); \
        sharedHeap->size = align_bytes; \
        sharedHeap->idx = 0; \
        sharedHeap->rank = rank; \
        CHECK_MPI( MPI_Win_create( sharedHeap, MYTHREAD == rank ? alloc_bytes : 0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &(sharedHeap->win) ); ); \
      } while (0)

    #define FREE_SHARED_HEAP(sharedHeap) \
    { \
      if (sharedHeap->idx != 0) assert( MYTHREAD == sharedHeap->rank); /* only 1 rank has allocation */ \
      CHECK_MPI( MPI_Win_free(&(sharedHeap->win)) ); \
      CHECK_MPI( MPI_Free_mem( sharedHeap ) ); \
      sharedHeap = NULL; \
    } while(0)

    #define ALLOC_FROM_SHARED_HEAP(sharedHeap, type, varname, count) \
      SharedPtr varname = __alloc_from_SharedHeap(sharedHeap, sizeof(type) * count)

    #define SHARED_MEMPUT(dst, src, size) __shared_memput(dst, src, size)
    #define SHARED_MEMGET(dst, src, size) __shared_memget(dst, src, size)

  #else // NOT MPI
    // OpenMP or fake it!

    struct {
      size_t size, idx;
    } _SharedHeap;
    typedef _SharedHeap *SharedHeap;
    typedef char * SharedPtr;

    static size_t __shared_atomic_fetchadd(SharedPtr ptr, size_t val) {
      #ifdef GCC_EXTENSION
        return __sync_fetch_and_add(ptr, val);
      #endif
      size_t t;
      #ifdef OPENMP_3_1
        #pragma omp atomic capture
      #else
        #pragma omp critical(_fetchadd)
      #endif
      { t = *ptr; *ptr += val; }
      return t;
    }
    #define ATOMIC_FETCHADD __shared_atomic_fetchadd
    #define ATOMIC_CSWAP TODO

    static SharedPtr __alloc_from_SharedHeap(SharedHeap sharedHeap, size_t size) {
      assert(sharedHeap);
      size = ALIGNED_MEM_SIZE(size);
      size_t idx = ATOMIC_FETCHADD(&(sharedHeap->idx), size); \
      if (idx + size > sharedHeap->size) DIE("Thread %d: attempt to allocate past size of SharedHeap (%lld of %lld)\n", (long long idx), (long long) sharedHeap->size);
      SharedPtr data = (SharedPtr) sharedHeap;
      return data + idx + ALIGNED_MEM_SIZE( sizeof(_SharedHeap) );
    }

    /* only MYTHREAD == rank will allocate the blocks of memory */
    #define INIT_SHARED_HEAP(sharedHeap, blocks, bytes, rank) \
      SharedHeap sharedHeap = NULL; \
      if (MYTHREAD == rank) { \
        size_t alignedBytes = ALIGNED_MEM_SIZE( bytes ) * blocks; \
        sharedHeap  = (SharedHeap) malloc( alignedBytes + ALIGNED_MEM_SIZE( sizeof(_SharedHeap) ) ); \
        if (sharedHeap == NULL) DIE("Could not allocate %lld bytes for SharedHeap\n", (long long) alignedBytes); \
        sharedHeap->size = alignedBytes; \
        sharedHeap->idx = 0; \
      }

    #define FREE_SHARED_HEAP(sharedHeap) \
      { \
        free(sharedHeap); \
        sharedHeap = NULL; \
      } while(0)

    #define ALLOC_FROM_SHARED_HEAP(sharedHeap, type, varname, count) \
        SharedPtr varname = __alloc_from_SharedHeap(sharedHeap, sizeof(type) * count)

    #define SHARED_MEMPUT(dst, src, size) memcpy(dst, src, size)
    #define SHARED_MEMGET(dst, src, size) memcpy(dst, src, size)
  #endif // NOT MPI

#endif // NOT UPC

#if defined (__cplusplus)
}
#endif

#endif // SHARED_HEAP_H_

