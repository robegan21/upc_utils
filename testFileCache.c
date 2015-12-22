#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "CommonParallel.h"
#include "Buffer.h"
#include "FileMap.h"

int main (int argc, char **argv) {

  INIT(argc, argv);

  int i, try;
  double init = NOW();
  if (!MYTHREAD) {
    printf("Starting with %d threads\n", THREADS);
    fflush(stdout);
  }
  BARRIER;

  for(i = 1; i < argc; i++) {
    double tstart = NOW();
    FileMap fm = initFileMap(argv[i], "r", MYTHREAD, THREADS);
    double topen = NOW();

    LOG(0,"Thread %d: Opened from %ld up through %ld (%ld). %0.3f s: %s\n", MYTHREAD, fm->myStart, fm->myEnd, fm->filesize, topen - tstart, argv[i] );
    BARRIER;

    double sec;
    sec = NOW() - tstart;
    if (!MYTHREAD) {
      SLOG(0,"Time to open %0.3f s\n\n", sec);
    }
    BARRIER;

    size_t lines = 0, bytes = 0;
    double tfirst = NOW();
    while (haveMoreFileMap(fm)) {
        char *line = getLineFileMap(fm);
        if (!line) {
          break;
        }
        lines++;
        bytes += getLengthBuffer(fm->buf);
    }
    double s_first = NOW() - tfirst;
    LOG(0, "Thread %d: Read %ld lines %ld bytes (%0.1f) in %0.3f s %0.3f MB/s\n", MYTHREAD, lines, bytes, ((double)bytes/(double)lines), s_first, bytes / s_first / 1048576.0);
    BARRIER;
    sec = NOW() - tfirst;
    SLOG(0,"Time to Read first %0.3f s %0.3f MB/s\n", sec, fm->filesize / sec / 1048576.0);

    for(try = 0; try < 1; try++) {
        rewindFileMap(fm);
        if (!MYTHREAD) {
          printf("\n");
        }
        BARRIER;
        lines = bytes = 0;
        double t = NOW();
        while (haveMoreFileMap(fm)) {
            char *line = getLineFileMap(fm);
            if (!line) {
              break;
            }
            lines++;
            bytes += getLengthBuffer(fm->buf);
        }
        sec = NOW() - t;
        LOG(0,"Thread %d: Try %d, Read %ld lines %ld bytes (%0.1f) in %0.3f s %0.3f MB/s\n", MYTHREAD, try, lines, bytes, ((double)bytes/(double)lines), sec, bytes / sec / 1048576.0);
        BARRIER;
        sec = NOW() - t;

        SLOG(0,"Time to read attempt %d: %0.3f s %0.3f MB/s\n", try, sec, (double)bytes*THREADS / sec / 1048576.0);
        BARRIER;
        if (!MYTHREAD) {
          printf("\n");
        }
        BARRIER;
    }

    freeFileMap(&fm);
    fflush(stderr);

    BARRIER;
  }

  SLOG(0, "Start to end time: %0.3f s\n", NOW() - init);
  FINALIZE();
  return 0;
}
