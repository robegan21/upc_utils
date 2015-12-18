EXECUTABLES = testFileCache testFileCache-nommap testFileCache-nomadvise testFileCache-nofadvise

UPCFLAGS := -O

all: $(EXECUTABLES)

%.o : %.upc
	upcc $(UPCFLAGS) -c -o $@ $<

%-nommap.o : %.upc
	upcc $(UPCFLAGS) -DNO_MMAP -c -o $@ $< 

%-nomadvise.o : %.upc
	upcc $(UPCFLAGS) -DNO_MADVISE -c -o $@ $< 

%-nofadvise.o : %.upc
	upcc $(UPCFLAGS) -DNO_FADVISE -DNO_MMAP -c -o $@ $< 

testFileCache : testFileCache.o Buffer.o FileMap.o
	upcc $(UPCFLAGS) -o $@ $^

testFileCache-nommap : testFileCache.o Buffer.o FileMap-nommap.o
	upcc $(UPCFLAGS) -DNO_MMAP -o $@ $^

testFileCache-nomadvise : testFileCache.o Buffer.o FileMap-nomadvise.o
	upcc $(UPCFLAGS) -DNO_MADVISE -o $@ $^

testFileCache-nofadvise : testFileCache.o Buffer.o FileMap-nofadvise.o
	upcc $(UPCFLAGS) -DNO_FADVISE -DNO_MMAP -o $@ $^

.PHONY: clean

clean: 
	rm -f *.o $(EXECUTABLES)
