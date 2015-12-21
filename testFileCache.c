#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "CommonParallel.h"
#include "Buffer.h"
#include "FileMap.h"

int main (int argc, char **argv) {

  INIT(argc, argv);
#pragma omp parallel
{
  int i, try;
  double init = NOW();
  if (!MYTHREAD) printf("Starting with %d threads\n", THREADS);

  for(i = 1; i < argc; i++) {
    double tstart = NOW();
    FileMap fm = initFileMap(argv[i], "r", MYTHREAD, THREADS);
    double topen = NOW();

    printf("Thread %d: Opened from %ld up through %ld (%ld). %0.3f s: %s\n", MYTHREAD, fm->myStart, fm->myEnd, fm->filesize, topen - tstart, argv[i] );
    fflush(stdout);
    BARRIER;
    double sec;
    sec = NOW() - tstart;
    if (!MYTHREAD) printf("Time to open %0.3f s\n\n", sec);
    fflush(stdout);
    BARRIER;

    size_t lines = 0, bytes = 0;
    double tfirst = NOW();
    while (haveMoreFileMap(fm)) {
        char *line = getLineFileMap(fm);
        if (!line) break;
        lines++;
        bytes += getLengthBuffer(fm->buf);
    }
    double s_first = NOW() - tfirst;
    printf("Thread %d: Read %ld lines %ld bytes (%0.1f) in %0.3f s %0.3f MB/s\n", MYTHREAD, lines, bytes, ((double)bytes/(double)lines), s_first, bytes / s_first / 1048576.0);
    fflush(stdout);
    BARRIER;
    fflush(stdout);
    sec = NOW() - tfirst;
    if (!MYTHREAD) printf("Time to Read first %0.3f s %0.3f MB/s\n", sec, fm->filesize / sec / 1048576.0);
    fflush(stdout);

    for(try = 0; try < 3; try++) {
        rewindFileMap(fm);
        if (!MYTHREAD) printf("\n");
        BARRIER;
        lines = bytes = 0;
        double t = NOW();
        while (haveMoreFileMap(fm)) {
            char *line = getLineFileMap(fm);
            if (!line) break;
            lines++;
            bytes += getLengthBuffer(fm->buf);
        }
        sec = NOW() - t;
        printf("Thread %d: Try %d, Read %ld lines %ld bytes (%0.1f) in %0.3f s %0.3f MB/s\n", MYTHREAD, try, lines, bytes, ((double)bytes/(double)lines), sec, bytes / sec / 1048576.0);
        fflush(stdout);
        BARRIER;
        sec = NOW() - t;

        if (!MYTHREAD) printf("Time to read %0.3f s %0.3f MB/s\n", sec, (double)bytes*THREADS / sec / 1048576.0);
        fflush(stdout);
        BARRIER;
        if (!MYTHREAD) printf("\n");
        fflush(stdout);

    }


    freeFileMap(&fm);

  }
  BARRIER;
  if (!MYTHREAD) 
        printf("%0.3f s\n", NOW() - init);
  
  FINALIZE();
}
  return 0;
}
