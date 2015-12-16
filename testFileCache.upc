#include <upc.h>
#include <upc_tick.h>
#include <stdio.h>
#include <assert.h>

#include "Buffer.h"
#include "FileMap.h"

#define UPC_TICKS_TO_SECS( t ) ( upc_ticks_to_ns( t ) / 1000000000.0)

int main (int argc, char **argv) {

    upc_tick_t tstart = upc_ticks_now();
    FileMap fm = initFileMap(argv[1], "r", MYTHREAD, THREADS);
    upc_tick_t topen = upc_ticks_now();

    printf("Thread %d: Reading from %ld up through %ld (%ld). %0.3f s\n", MYTHREAD, fm->myStart, fm->myEnd, fm->filesize, UPC_TICKS_TO_SECS(topen - tstart) );
    fflush(stdin);
    upc_barrier;
    upc_barrier;

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
    upc_barrier;

    
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
        double sec = UPC_TICKS_TO_SECS(upc_ticks_now() - t);
        printf("Thread %d: Try %d, Read %ld lines %ld bytes (%0.1f) in %0.3f s %0.3f MB/s\n", MYTHREAD, try, lines, bytes, ((double)bytes/(double)lines), sec, bytes / sec / 1048576.0);
        fflush(stdin);
        upc_barrier;
    }


    freeFileMap(&fm);
    return 0;
}
