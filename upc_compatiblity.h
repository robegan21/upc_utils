#ifndef UPC_COMPATIBILITY_H
#define UPC_COMPATIBILITY_H

#include <upc.h>
#include <assert.h>

#ifndef USE_BUPC
#ifndef USE_CRAY_UPC
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
#   if __UPC_VERSION__ < 201311L
#     define UPC_TICK_T bupc_tick_t
#     define UPC_TICKS_NOW bupc_ticks_now
#     define UPC_TICKS_TO_SECS( t ) (bupc_ticks_to_us( t ) / 1000000.0)
#   else
#     include <upc_tick.h>
#     define UPC_TICK_T upc_tick_t
#     define UPC_TICKS_NOW upc_ticks_now
#     define UPC_TICKS_TO_SECS( t ) (upc_ticks_to_ns( t ) / 1000000000.0)
#   endif
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
#   define UPC_TICKS_TO_SECS( t ) (upc_ticks_to_ns( t ) / 1000000000.0)
#endif

// This check is necessary to ensure that shared pointers of 16 bytes are unlikely to be corrupted.
// 8 byte pointers will never be subject to a race condition
#define IS_VALID_UPC_PTR( ptr ) ( sizeof(shared void *) == 8 ? 1 : ( ptr == NULL || ( upc_threadof(ptr) >= 0 && upc_threadof(ptr) < THREADS && upc_phaseof(ptr) >= 0 && upc_phaseof(ptr) < BS && upc_addrfield(ptr) < 17179869184 ) ) )


#endif // UPC_COMPATIBLITY_H

