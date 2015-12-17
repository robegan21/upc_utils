EXECUTABLES = testFileCache testFileCache-nommap

UPCFLAGS := -O

all: $(EXECUTABLES)

%.o : %.upc
	upcc $(UPCFLAGS) -c -o $@ $<

%-nommap.o : %.upc
	upcc $(UPCFLAGS) -DNO_MMAP -c -o $@ $< 

testFileCache : testFileCache.o Buffer.o FileMap.o
	upcc $(UPCFLAGS) -o $@ $^

testFileCache-nommap : testFileCache.o Buffer.o FileMap-nommap.o
	upcc $(UPCFLAGS) -o $@ $^

.PHONY: clean

clean: 
	rm -f *.o $(EXECUTABLES)
