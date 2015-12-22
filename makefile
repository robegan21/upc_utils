TYPES= upc mpi omp
EXECUTABLES = testFileCache
EXECUTABLE_BUILDS = $(foreach t, $(TYPES), $(foreach e, $(EXECUTABLES), $(e)-$(t) ) )

CC = gcc
MPICC = mpicc
CFLAGS := -O2 -DNDEBUG -DNO_MMAP
UPCFLAGS := -O -DNDEBUG -DNO_MMAP
#CFLAGS := -O -DDEBUG -g -DNO_MMAP
#UPCFLAGS := -O -g -DNO_MMAP

all: $(EXECUTABLE_BUILDS)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%-upc.o : %.c
	upcc $(UPCFLAGS) -c -o $@ $<

%-omp.o : %.c
	$(CC) $(CFLAGS) -fopenmp -c -o $@ $<

%-mpi.o : %.c
	$(MPICC) -DUSE_MPI $(CFLAGS) -c -o $@ $<

testFileCache-mpi : testFileCache-mpi.o Buffer.o FileMap-mpi.o
	$(MPICC) -DUSE_MPI $(CFLAGS) -o $@ $^

testFileCache-omp : testFileCache-omp.o Buffer.o FileMap-omp.o
	$(CC) $(CFLAGS) -fopenmp -o $@ $^

testFileCache-upc : testFileCache-upc.o Buffer.o FileMap-upc.o
	upcc $(UPCFLAGS) -o $@ $^

#%-nommap.o : %.upc
#	upcc $(UPCFLAGS) -DNO_MMAP -c -o $@ $< 
#
#%-nomadvise.o : %.upc
#	upcc $(UPCFLAGS) -DNO_MADVISE -c -o $@ $< 
#
#%-nofadvise.o : %.upc
#	upcc $(UPCFLAGS) -DNO_FADVISE -DNO_MMAP -c -o $@ $< 
#
#testFileCache : testFileCache.o Buffer.o FileMap.o
#	upcc $(UPCFLAGS) -o $@ $^
#
#testFileCache-nommap : testFileCache.o Buffer.o FileMap-nommap.o
#	upcc $(UPCFLAGS) -DNO_MMAP -o $@ $^
#
#testFileCache-nomadvise : testFileCache.o Buffer.o FileMap-nomadvise.o
#	upcc $(UPCFLAGS) -DNO_MADVISE -o $@ $^
#
#testFileCache-nofadvise : testFileCache.o Buffer.o FileMap-nofadvise.o
#	upcc $(UPCFLAGS) -DNO_FADVISE -DNO_MMAP -o $@ $^
#

.PHONY: clean

clean: 
	rm -f *.o $(EXECUTABLE_BUILDS)
