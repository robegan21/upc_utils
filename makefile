TYPES= mpi omp
#TYPES= upc mpi omp
EXECUTABLES = testFileCache
EXECUTABLE_BUILDS = $(foreach t, $(TYPES), $(foreach e, $(EXECUTABLES), $(e)-$(t) ) ) testFileCache-mmap-omp

CC = gcc
MPICC = mpicc
CFLAGS := -O2 -DNDEBUG -DNO_MMAP
UPCFLAGS := -O -DNDEBUG -DNO_MMAP

CFLAGS_MMAP := -O -DDEBUG -g
UPCFLAGS_MMAP := -O -g

CFLAGS_DEBUG := -O -DDEBUG -g -DNO_MMAP
UPCFLAGS_DEBUG := -O -g -DNO_MMAP

all: $(EXECUTABLE_BUILDS)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%-upc.o : %.c
	upcc $(UPCFLAGS) -c -o $@ $<

%-omp.o : %.c
	$(CC) $(CFLAGS) -fopenmp -c -o $@ $<

%-mmap-omp.o : %.c
	$(CC) $(CFLAGS_MMAP) -fopenmp -c -o $@ $<

%-mpi.o : %.c
	$(MPICC) -DUSE_MPI $(CFLAGS) -c -o $@ $<

testFileCache-mpi : testFileCache-mpi.o Buffer.o FileMap-mpi.o
	$(MPICC) -DUSE_MPI $(CFLAGS) -o $@ $^

testFileCache-omp : testFileCache-omp.o Buffer.o FileMap-omp.o
	$(CC) $(CFLAGS) -fopenmp -o $@ $^

testFileCache-mmap-omp : testFileCache-mmap-omp.o Buffer.o FileMap-mmap-omp.o
	$(CC) $(CFLAGS_MMAP) -fopenmp -o $@ $^

testFileCache-upc : testFileCache-upc.o Buffer.o FileMap-upc.o
	upcc $(UPCFLAGS) -o $@ $^

.PHONY: clean

clean: 
	rm -f *.o $(EXECUTABLE_BUILDS)
