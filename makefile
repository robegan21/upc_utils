EXECUTABLES = testFileCache testFileCache2

UPCFLAGS := -O

all: $(EXECUTABLES)

%.o : %.upc
	upcc $(UPCFLAGS) -c $ -o $@ $<

testFileCache : testFileCache.o Buffer.o FileMap.o
	upcc $(UPCFLAGS) -o $@ $^

testFileCache2 : testFileCache.o Buffer.o FileMap2.o
	upcc $(UPCFLAGS) -o $@ $^

.PHONY: clean

clean: 
	rm -f *.o $(EXECUTABLES)
