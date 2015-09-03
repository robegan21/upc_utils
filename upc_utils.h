//
//  upc_utils.h
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

#ifndef _UPC_UTILS_H_
#define _UPC_UTILS_H_

#include <assert.h>
#include <stdio.h>
#include <upc.h>

#ifndef VERBOSE
#define VERBOSE 0
#endif

/* TODO: Define different block sizes according to data structure */
#define BS 1

FILE *mylog;
#define LOG(...) fprintf(mylog == NULL ? stderr : mylog, __VA_ARGS__)
#define DEBUG_LOG(...) if (VERBOSE>0) LOG(__VA_ARGS__)
#define DEBUG_LOG2(...) if (VERBOSE>1) LOG(__VA_ARGS__)

#ifndef USE_CRAY_UPC
#ifndef USE_BUPC
#define USE_BUPC
#endif
#endif

#if defined USE_BUPC
#   define UPC_POLL bupc_poll()
#   define UPC_INT64_T int64_t
#   define UPC_ATOMIC_FADD_I64 bupc_atomicI64_fetchadd_strict
#   define UPC_ATOMIC_CSWAP_I64 bupc_atomicI64_cswap_strict
#   define USED_FLAG_TYPE int32_t
#   define UPC_ATOMIC_CSWAP_USED_FLAG bupc_atomicI_cswap_strict
#   define UPC_TICK_T bupc_tick_t
#   define UPC_TICKS_NOW bupc_ticks_now
#   define UPC_TICKS_TO_SECS( t ) (bupc_ticks_to_ns( t ) / 1000000000.0)
#elif defined USE_CRAY_UPC
#include <upc_tick.h>
#   define UPC_POLL /* noop */
#   define UPC_INT64_T long
#   define UPC_ATOMIC_FADD_I64 _amo_afadd_upc
#   define UPC_ATOMIC_CSWAP_I64 _amo_acswap_upc
#   define USED_FLAG_TYPE UPC_INT64_T
#   define UPC_ATOMIC_CSWAP_USED_FLAG UPC_ATOMIC_CSWAP_I64
#   define UPC_TICK_T upc_tick_t
#   define UPC_TICKS_NOW upc_ticks_now
#   define UPC_TICKS_TO_SECS( t ) ( upc_ticks_to_ns( t ) / 1000000000.0)
#endif

// This check is necessary to ensure that shared pointers of 16 bytes are unlikely to be corrupted.
// 8 byte pointers will never be subject to a race condition
#define IS_VALID_UPC_PTR( ptr ) ( sizeof(shared void *) == 8 ? 1 : ( ptr == NULL || ( upc_threadof(ptr) >= 0 && upc_threadof(ptr) < THREADS && upc_phaseof(ptr) >= 0 && upc_phaseof(ptr) < BS && upc_addrfield(ptr) < 17179869184 ) ) )

#define loop_until( stmt, cond ) \
{ \
	long attempts = 0; \
	do { \
		stmt; \
		if ( cond ) break; \
		UPC_POLL; \
		if (++attempts % 100000 == 0) LOG("Thread %d: possible infinite loop in loop_until(" #stmt ", " #cond "): %s:%u\n", MYTHREAD, __FILE__, __LINE__); \
	} while( 1 ); \
	assert( cond ); \
}

#ifndef REMOTE_ASSERT
#define REMOTE_ASSERT 0
#endif

#if REMOTE_ASSERT != 0
#define remote_assert( cond ) loop_until( ((void)0), cond ); assert( cond );
#else
#define remote_assert( cond ) /* noop(cond) */
#endif

#endif
