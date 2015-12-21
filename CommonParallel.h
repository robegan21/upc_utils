#ifndef COMMON_PARALLEL_H_
#define COMMON_PARALLEL_H_

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifdef __UPC__
  #include <upc.h>
#elif defined _OPENMP
  #include <omp.h>
#elif defined USE_MPI
  #include <mpi.h>
#endif

#if defined (__cplusplus)
extern "C" {
#endif


/* First defined the EXIT_FUNC that will be used */
#ifdef __UPC__
  #define EXIT_FUNC(code) upc_global_exit(code)
  #define INIT(argc, argv)
  #define FINALIZE()

#else
  #ifdef MPI_VERSION
    #define EXIT_FUNC(code) do { MPI_Abort(MPI_COMM_WORLD, code); exit(code); } while (0)
    #define INIT(argc, argv) MPI_Init(&argc, &argv)
    #define FINALIZE() MPI_Finalize()
  #else
    #define EXIT_FUNC(x) exit(x)
    #define INIT(argc, argv) {}
    #define FINALIZE() {}
  #endif
#endif

#ifndef VERBOSE
#define VERBOSE 0
#endif

#ifndef LOG
static void writeMyLog(int level, const char *fmt, ...);
#define LOG(level, fmt, ...) if (VERBOSE >= level) { writeMyLog(level, fmt, ##__VA_ARGS__); } 
#endif

#ifndef DIE
static inline int *hasMyLog();
static inline void closeMyLog();
#define DIE(fmt,...)                                                                                                    \
    do {                                                                \
        fprintf(stderr, COLOR_RED "Thread %d, DIE [%s:%d]: " COLOR_NORM fmt,             \
                MYTHREAD, __FILE__, __LINE__, ##__VA_ARGS__);           \
        fflush(stderr); \
        if (*hasMyLog()) { \
            LOG(0, COLOR_RED "DIE [%s:%d]: " COLOR_NORM fmt,             \
                __FILE__, __LINE__, ##__VA_ARGS__); \
            closeMyLog(); \
        } \
        EXIT_FUNC(1); \
    } while (0)
#endif

#ifndef CHECK_ERR
#define CHECK_ERR(cmd, val)                                                  \
    do {                                                                \
        int err;                                                        \
        if ((err = cmd) != val)                                           \
            DIE("Thread %d, " #cmd " failed, error %s\n", MYTHREAD, strerror(err)); \
    } while (0)
#endif

#ifdef __UPC__

  // UPC 
  #pragma message "Using UPC CommonParallel.h"
  #include "upc_compatiblity.h"

  #define BARRIER upc_barrier
  #define NOW() UPC_TICKS_TO_SECS( UPC_TICKS_NOW() )

#else // NOT UPC

  #ifdef MPI_VERSION
    // // MPI, ensure mpi.h is loaded
    #pragma message "Using MPI CommonParallel.h"

    #define CHECK_MPI(x) CHECK_ERR(x, MPI_SUCCESS)
    static inline int __get_THREADS() {
        int size;
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        return size;
    }
    static inline int __get_MYTHREAD() {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        return rank;
    }
    #define BARRIER do { LOG(3, "Starting barrier at %d %s\n", __LINE__, __FILE__); CHECK_MPI(MPI_Barrier(MPI_COMM_WORLD)); } while (0)
    #define NOW() MPI_Wtime()

  #else
    // OpenMP or bust!

    #ifdef _OPENMP
      #pragma message "Using OpenMP in CommonParallel.h"
      // OpenMP, ensure omp.h is loaded

    #else

      #warning "No Parallel framework has been detected, assuming serial execution, making dummy OpenMP API functions in CommonParallel.h"
    
      #define omp_get_thread_num() 0
      #define omp_get_num_threads() 1
      #define omp_get_max_threads() 1
      #define omp_in_parallel() 0

      static void omp_set_num_threads(int i) {}
      static void omp_set_nested(int i) {}

    #endif

    static inline int __get_THREADS() { 
        static int _threads = 0;
        if (_threads <= 0) {
            if (omp_in_parallel()) {
              #pragma omp serial
              _threads = omp_get_num_threads();
              #pragma omp barrier
            } else {
              #pragma omp parallel
              {
                #pragma omp serial
                _threads = omp_get_num_threads();
              }
            }
        }
        assert(_threads > 0);
        return _threads;
    }
             
    static inline int __get_MYTHREAD() {
         return omp_get_thread_num();
    }

    static inline void __barrier() {
         #pragma omp barrier
    }
    static inline double __get_seconds() {
        struct timeval tv;
        gettimeofday(&tv,NULL);
        return ((long long) tv.tv_usec + 1000000 * (long long) tv.tv_sec) / (double) 1000000.0;
    }

    #define BARRIER __barrier()
    #define NOW() __get_seconds()

  #endif

  #define THREADS (__get_THREADS())
  #define MYTHREAD (__get_MYTHREAD())

#endif


#ifndef _MESSAGE_MACROS
  #define _MESSAGE_MACROS

  #define COLOR_NORM   "\x1B[0m"
  #define COLOR_RED   "\x1B[91m"
  #define COLOR_GREEN  "\x1B[32m"

  static inline int *hasMyLog() {
    static int _log = 0;
    return &_log;
  }
  typedef struct { FILE *f; } FILE2;
  static inline FILE2 *_getMyLog() {
    #ifdef __UPC__
      static shared[] FILE2 *_mylog = NULL;
      if (_mylog == NULL) {
        BARRIER;
        _mylog = (shared[] FILE2 *) upc_all_alloc(THREADS, sizeof(FILE2));
        assert(upc_threadof(&(_mylog[MYTHREAD])) == MYTHREAD);
        FILE2 *f = (FILE2 *) &_mylog[MYTHREAD];
        assert(f != NULL);
        f->f = stderr;
        BARRIER;
        (*hasMyLog())++;
      }
      assert(_mylog != NULL);
      assert(_mylog[MYTHREAD].f != (FILE*) NULL);
      assert(upc_threadof(_mylog + MYTHREAD) == MYTHREAD);
      FILE2 *f = (FILE2 *) &_mylog[MYTHREAD];
      return (FILE2*) f;
    #else
      static FILE2 *_mylog = NULL;
      #ifdef MPI_VERSION
        if (_mylog == NULL) {
          _mylog = (FILE2*) calloc(1, sizeof(FILE2));
          assert(_mylog);
          _mylog->f = stderr;
          BARRIER;
          (*hasMyLog())++;
        }
        assert(_mylog != NULL && _mylog->f != NULL);
        return _mylog; 
      #else
        if (_mylog == NULL) {
          BARRIER;
          if (!MYTHREAD) {
            _mylog = (FILE2*) calloc(THREADS, sizeof(FILE2));
          }
          BARRIER;
          assert(_mylog != NULL && _mylog[MYTHREAD].f == NULL);
          _mylog[MYTHREAD].f = stderr;
          (*hasMyLog())++;
        }
        assert(_mylog != NULL && _mylog[MYTHREAD].f != NULL);
        return _mylog + MYTHREAD;
      #endif 
      
    #endif
  }
  static inline FILE *getMyLog() {
      FILE2 *_mylog = _getMyLog();
      assert(_mylog != NULL);
      FILE *mylog = (FILE*) _mylog->f;
      assert(mylog != NULL);
      return mylog;
  }
  static inline void closeMyLog() {
      if (*hasMyLog()) {
          FILE *mylog = getMyLog();
          if (mylog != stderr && mylog != stdout) {
              fclose(getMyLog());
              _getMyLog()->f = stderr;
          }
      }
  }

  static inline void _setMyLog(const char *myfile) {
      FILE2 *mylog = _getMyLog();
      assert(mylog != NULL);
      mylog->f = fopen(myfile, "w+");
      if (mylog->f == NULL) DIE("Could not open %s for writing to a log!\n", myfile);
      assert(((FILE*) mylog->f) == getMyLog());
      BARRIER;
  }

  static inline void setMyLog(const char *prefix) {
      char myfile[384];
      snprintf(myfile, 384, "%s.%6dof%6d", prefix, MYTHREAD, THREADS);
      _setMyLog(myfile);
  }

  static void writeMyLog(int level, const char *fmt, ...) {
    time_t rawtime; struct tm *timeinfo; 
    time( &rawtime ); timeinfo = localtime( &rawtime ); 
    char newfmt[1024];
    snprintf(newfmt, 1024, "Thread %d [%s %s]: %s", MYTHREAD,
            level == 0 ? "ALL" : (level == 1 ? "INFO" : "DEBUG" "level"), asctime(timeinfo), fmt);

    va_list args;
    va_start(args, fmt);
    vfprintf(getMyLog(), newfmt, args);
    va_end(args);
  } 

  #define LOG2(level, fmt, ...) fprintf(getMyLog(), fmt, ##__VA_ARGS__)

  #define LOG_FLUSH(level, fmt, ...) do { LOG(level, fmt, ##__VA_ARGS__); fflush(getMyLog()); } while (0)

  #define SDIE(fmt,...)                                                                                                   \
    do {                                                                \
        if (!MYTHREAD) {                                                 \
            fprintf(stderr, COLOR_RED "Thread %d, DIE [%s:%d]: " fmt COLOR_NORM, \
                    MYTHREAD, __FILE__, __LINE__, ##__VA_ARGS__);       \
            fflush(stderr); \
        } \
        if (*hasMyLog()) { \
            LOG(0, COLOR_RED "DIE [%s:%d]: " COLOR_NORM fmt,             \
                __FILE__, __LINE__, ##__VA_ARGS__); \
            closeMyLog(); \
        } \
        EXIT_FUNC(1);                                                   \
    } while (0)

  #define WARN(fmt,...)                                               \
    do {                                                            \
        fprintf(stderr, COLOR_RED "Thread %d, WARN [%s:%d]: " fmt COLOR_NORM, \
                MYTHREAD, __FILE__, __LINE__, ##__VA_ARGS__);       \
    } while (0)

#endif // _MESSAGE_MACROS


#if defined (__cplusplus)
}
#endif

#endif // COMMON_PARALLEL_H_
