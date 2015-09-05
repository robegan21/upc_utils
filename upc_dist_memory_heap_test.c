//
//  upc_shared_memory_heap_test.c
//  
//
//  Created by Rob Egan on 12/22/14.
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
	int thread;
	long long idx;
	HeapPos hp;
};
typedef struct _mytype mytype;
typedef shared[] mytype *sharedMyTypePtr;

//UPC_SHARED_MEMORY_HEAP_INIT_STATIC(stat, mytype)
//UPC_SHARED_MEMORY_HEAP_INIT_DYNAMIC(dyn, mytype)

mytype getValue(thread, idx, size) {
	mytype x;
	x.thread = thread;
	x.idx = idx;
	x.hp.heapPos = 0;
	return x;
}

int main() {
	/* initialize memory */
	long long mysize = 128;
	long long mytypesize = sizeof(mytype);
	mytype *x = (mytype*) malloc( mysize * mytypesize);;
        long long step = 1;
	printf("Thread %d: mytypesize=%lld\n", MYTHREAD, mytypesize);
        for(long long i = 0; i < (mysize+step-1)/step; i++) {
		for(long long j = 0; j < step; j++) {
			x[i*step+j] = getValue(MYTHREAD, i*step+j, mysize);
			assert(x[i*step+j].thread == MYTHREAD);
			assert(x[i*step+j].hp.heapPos == 0);
			LOG("Thread %d: constructed i=%lld j=%lld idx=%lld, x.thread=%d x.idx=%lld\n", MYTHREAD, i, j, i*step+j, x[i*step+j].thread, x[i*step+j].idx);
		}
        }

	// test single allocation event
	DistHeapHandlePtr dh = constructDistHeap(mytypesize*mysize);	
        
	SharedHeapTypePtr lastPos = NULL;
	for(long long i = 0; i < (mysize+step-1)/step; i++) {
		int destthread = (MYTHREAD+i)%THREADS;
		for(long long j = 0; j < step ; j++) {
			LOG("Thread %d: Sending i=%lld j=%lld idx=%lld, x.thread=%d x.idx=%lld\n", MYTHREAD, i, j, i*step+j, x[i*step+j].thread, x[i*step+j].idx);
			assert(x[i*step+j].thread == MYTHREAD);
			assert(x[i*step+j].hp.heapPos == 0);
			x[i*step+j].hp = getHeapPosFromDistHeapHandle(dh, lastPos == NULL ? NULL : lastPos + j);
		}
		while( NULL == (lastPos = tryPutData(dh, destthread, (HeapType*) (x+i*step), step*mytypesize))) { 
			LOG("Thread %d: Attempt to put %lld elements %lld bytes to %d failed!\n", MYTHREAD, step, step*mytypesize, destthread);
		}
		assert(lastPos != NULL);
		assert(upc_threadof(lastPos) == destthread);
		for(long long j = 0; j < step ; j++) {
			assert( ((sharedMyTypePtr) (lastPos+j))->thread == MYTHREAD);
			assert( ((sharedMyTypePtr) (lastPos+j))->idx == i*step+j);
		}
        }
	distHeapBarrier(dh);
	upc_fence;
	upc_barrier;
	// validate contents of mythread...
	assert(upc_threadof(dh->distHeapData + MYTHREAD) == MYTHREAD);
	SharedHeapAllocationPtr heapHead = dh->distHeapData[MYTHREAD].heapHead;
	assert(upc_threadof(heapHead) == MYTHREAD);
	assert(dh->distHeapData[MYTHREAD].heapHead == dh->distHeapData[MYTHREAD].activeHeap); // i.e. only one allocation event
	assert(dh->distHeapData[MYTHREAD].heapHead->size > 0);
	assert(dh->distHeapData[MYTHREAD].heapHead->offset > 0);
	assert(dh->distHeapData[MYTHREAD].heapHead->offset == dh->distHeapData[MYTHREAD].heapHead->confirmed);
	SharedHeapIterator begin = getHeapBegin(dh, MYTHREAD), end = getHeapEnd(dh,MYTHREAD);
	assert(begin->heap == dh->distHeapData[MYTHREAD].heapHead);
	assert(end->heap == dh->distHeapData[MYTHREAD].activeHeap);
	assert(begin->heap == end->heap); // only 1 allocation event
	assert(begin->idx == getHeapAllocationDataStart());
	assert(end->idx == end->heap->confirmed);
	LOG("Thread %d: begin: %lld confirmed:%lld size:%lld offset:%lld heapOffset:%lld\n", MYTHREAD, begin->idx, begin->heap->confirmed, begin->heap->size, begin->heap->offset, begin->heap->heapOffset);
	LOG("Thread %d: end: %lld confirmed:%lld size:%lld offset: %lld heapOffset:%lld\n", MYTHREAD, end->idx, end->heap->confirmed, end->heap->size, end->heap->offset, end->heap->heapOffset);
	assert(end->idx > mytypesize * mysize);
	long long idx = 0;
	while ( heapIteratorHasNext(begin, end, mytypesize) ) {
		SharedHeapTypePtr ptr = getSharedHeapTypePtr( begin );	
		assert( upc_threadof(ptr) == MYTHREAD );
		mytype *myreceived = (mytype*) ptr;
		int fromThread = (THREADS*mysize+MYTHREAD-(myreceived->idx/step))%THREADS;
		LOG("Thread %d: iterator idx=%lld, received.thread=%d, received.idx=%lld, expecedFrom:%d\n", MYTHREAD, idx, myreceived->thread, myreceived->idx, fromThread);
		assert(myreceived->thread == fromThread);
		incrementHeapIterator( begin, mytypesize );
		idx++;
	}	
	upc_barrier;

	begin = getHeapBegin(dh, MYTHREAD);
	mytype *myreceived2 = (mytype*) getSharedHeapTypePtr( begin );
	for(long long i = 0; i < (mysize+step-1)/step; i++) {
//		int fromThread = (THREADS*mysize+MYTHREAD-i)%THREADS;
		for(long long j = 0; j < step; j++) {
			int fromThread = (THREADS*mysize+MYTHREAD-(myreceived2[i*step+j].idx/step))%THREADS;
			LOG("Thread %d: received i=%lld j=%lld idx=%lld, received.thread=%d, received.idx=%lld, expecedFrom:%d\n", MYTHREAD, i, j, i*step+j, myreceived2[i*step+j].thread, myreceived2[i*step+j].idx, fromThread);
			assert(myreceived2[i*step+j].thread == fromThread);
		}
	}
        destroyHeapIterator(&begin);
	destroyHeapIterator(&end); 
	destroyDistHeap(&dh);

	return 0;
}
