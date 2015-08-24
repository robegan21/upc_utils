//
//  upc_shared_memory_heap_test.c
//  
//
//  Created by Rob Egan on 12/22/14.
//
//

// always compile with assertions on
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <upc.h>
#include <upc_utils.h>
#include <upc_nb_utils.h>

#include "upc_dist_memory_heap.h"

struct _mytype {
	int a, b, c;
	HeapPos hp;
};
typedef struct _mytype mytype;
typedef shared[] mytype *sharedMyTypePtr;

//UPC_SHARED_MEMORY_HEAP_INIT_STATIC(stat, mytype)
//UPC_SHARED_MEMORY_HEAP_INIT_DYNAMIC(dyn, mytype)

mytype getValue(thread, idx, size) {
	mytype x;
	x.a = thread * size + idx;
	x.b = x.a * 10;
	x.c = x.b * 10 + thread - idx; 
	x.hp.heapPos = 0;
	return x;
}

int main() {
	/* initialize memory */
	long long mysize = 1000;
	long long mytypesize = sizeof(mytype);
	mytype *x = (mytype*) malloc( mysize * sizeof(mytypesize));;
        for(int i = 0; i < mysize; i++) {
            x[i] = getValue(MYTHREAD, i, mysize);
        }

	// test single allocation event
	DistHeapHandlePtr dh = constructDistHeap(mytypesize*mysize);	
        
        long long step = 1;
	for(int i = 0; i + step <= mysize; i+=step) {
		int thread = (MYTHREAD+i)%THREADS;
		SharedHeapTypePtr lastPos;
		while( NULL == (lastPos = tryPutData(dh, thread, (HeapType*) (x+i), step*mytypesize))) { 
			LOG("Attempt to put %lld to %d failed!\n", step*mytypesize, thread);
		}
		assert( ((sharedMyTypePtr) lastPos)->a == x[i].a );
		assert( ((sharedMyTypePtr) lastPos)->b == x[i].b );
		assert( ((sharedMyTypePtr) lastPos)->c == x[i].c );
		assert( ((sharedMyTypePtr) lastPos)->hp.heapPos == x[i].hp.heapPos );
        }
	distHeapBarrier(dh);
            
	// validate contents of mythread...
	assert(upc_threadof(dh->distHeapData + MYTHREAD) == MYTHREAD);
	SharedHeapAllocationPtr heapHead = dh->distHeapData[MYTHREAD].heapHead;
	assert(upc_threadof(heapHead) == MYTHREAD);
	mytype *myreceived = (mytype*) heapHead;
	assert(dh->distHeapData[MYTHREAD].heapHead == dh->distHeapData[MYTHREAD].heapTail); // i.e. only one allocation event
	for(int i = 0; i + step <= mysize; i+=step) {
		int fromThread = (MYTHREAD-i)%THREADS;
		mytype expected = getValue(fromThread, i, mysize);
		assert( expected.a == myreceived[i].a );
		assert( expected.b == myreceived[i].b );
		assert( expected.c == myreceived[i].c );
	}
         
	destroyDistHeap(&dh);

/* 
OLD VERSION
	stat_memory_heap_t *stat_memory = stat_init_heap(1000);
	stat_shared_buffer_t myheap = stat_memory->heap_ptrs[MYTHREAD];
	assert(upc_threadof( myheap ) == MYTHREAD);
	assert(myheap->tracker.index == 0);
	assert(myheap->tracker.written == 0);
	assert(myheap->tracker.size == 1000);
	assert(myheap->tracker.tag == 0);
	
	for(i = 0; i < 1000; i++) {
		x.a += i;
		x.b += i;
		x.c += i;
		stat_batched_put_buffer(stat_memory, (MYTHREAD+i)%THREADS, &x);
	}
	
	stat_finish_processing( stat_memory );
	assert(myheap->tracker.index == 1000);
	assert(myheap->tracker.written == 1000);
	assert(myheap->tracker.size == 1000);
	//assert(myheap->tracker.tag == 0);
	
	stat_shared_type tmp = &(myheap->buffer_start);
	for(i = 0; i < 1000; i++) {
		assert(tmp->a != 0);
		tmp++;
	}
	
	stat_free_heap( &stat_memory );

*/
	
	return 0;
}
