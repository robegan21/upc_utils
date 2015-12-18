#include <upc.h>
#include <upc_tick.h>
#include <stdio.h>
#include <assert.h>

#include "Buffer.h"
#include "FileMap.h"

#define UPC_TICKS_TO_SECS( t ) ( upc_ticks_to_ns( t ) / 1000000000.0)

int main (int argc, char **argv) {

  upc_tick_t init = upc_ticks_now();
  for(int i = 1; i < argc; i++) {
    upc_tick_t tstart = upc_ticks_now();
    FileMap fm = initFileMap(argv[i], "r", MYTHREAD, THREADS);
    upc_tick_t topen = upc_ticks_now();

    printf("Thread %d: Opened from %ld up through %ld (%ld). %0.3f s: %s\n", MYTHREAD, fm->myStart, fm->myEnd, fm->filesize, UPC_TICKS_TO_SECS(topen - tstart), argv[i] );
    fflush(stdout);
    upc_barrier;
    double sec;
    sec = UPC_TICKS_TO_SECS(upc_ticks_now() - tstart);
    if (!MYTHREAD) printf("Time to open %0.3f s\n\n", sec);
    fflush(stdout);

    size_t lines = 0, bytes = 0;
    upc_tick_t tfirst = upc_ticks_now();
    while (haveMoreFileMap(fm)) {
        char *line = getLineFileMap(fm);
        if (!line) break;
        lines++;
        bytes += getLengthBuffer(fm->buf);
    }
    double s_first = UPC_TICKS_TO_SECS(upc_ticks_now() - tfirst);
    printf("Thread %d: Read %ld lines %ld bytes (%0.1f) in %0.3f s %0.3f MB/s\n", MYTHREAD, lines, bytes, ((double)bytes/(double)lines), s_first, bytes / s_first / 1048576.0);
    fflush(stdout);
    upc_barrier;
    sec = UPC_TICKS_TO_SECS(upc_ticks_now() - tfirst);
    if (!MYTHREAD) printf("Time to Read first %0.3f s %0.3f MB/s\n", sec, fm->filesize / sec / 1048576.0);
    fflush(stdout);

    for(int try = 0; try < 3; try++) {
        rewindFileMap(fm);
        if (!MYTHREAD) printf("\n");
        upc_barrier;
        lines = bytes = 0;
        upc_tick_t t = upc_ticks_now();
        while (haveMoreFileMap(fm)) {
            char *line = getLineFileMap(fm);
            if (!line) break;
            lines++;
            bytes += getLengthBuffer(fm->buf);
        }
        sec = UPC_TICKS_TO_SECS(upc_ticks_now() - t);
        printf("Thread %d: Try %d, Read %ld lines %ld bytes (%0.1f) in %0.3f s %0.3f MB/s\n", MYTHREAD, try, lines, bytes, ((double)bytes/(double)lines), sec, bytes / sec / 1048576.0);
        fflush(stdout);
        upc_barrier;
        sec =  UPC_TICKS_TO_SECS(upc_ticks_now() - t);

        if (!MYTHREAD) printf("Time to read %0.3f s %0.3f MB/s\n", sec, (double)bytes*THREADS / sec / 1048576.0);
        fflush(stdout);
        upc_barrier;
        if (!MYTHREAD) printf("\n");
        fflush(stdout);

    }


    freeFileMap(&fm);

  }
  upc_barrier;
  if (!MYTHREAD) 
        printf("%0.3f s\n", UPC_TICKS_TO_SECS(upc_ticks_now() - init));
  
  return 0;
}
