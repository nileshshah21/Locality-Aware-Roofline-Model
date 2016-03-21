#Compilation
OPTIONS=-g -Wall -Werror -Wextra
CC=gcc
OMP_FLAG=-fopenmp
HWLOC_FLAG=-lhwloc

#Vectorisation type among {AVX512, AVX, SSE, SSE2, DBL}
SIMD=AVX
#Duration time of each sample (millisecond)
DUR=1
#Type of parallelisme among {SEQ, OMP}
PAR=OMP
#Machinde specific code implementing MSC/MSC.h
MSC=MSC/intel.c

DEFINES=-DUSE_$(SIMD) -DBENCHMARK_MIN_DUR=$(DUR) -DUSE_$(PAR) -DCC=$(CC)

all: main

MSC.o: $(MSC)
	$(CC) -O0 $(OPTIONS) $(DEFINES) -c -o $@ $^ -ldl $(OMP_FLAG)

MSC.s: $(MSC)
	$(CC) -O0 $(OPTIONS) $(DEFINES) -S -o $@ $^ -ldl $(OMP_FLAG)

roofline.o: roofline.c
	$(CC) $(OPTIONS) -pedantic-errors $(DEFINES) -c -o $@ $^ $(OMP_FLAG)

utils.o: utils.c
	$(CC) $(OPTIONS) -pedantic-errors -c -o $@ $^ -lm $(HWLOC_FLAG)

stats.o: stats.c
	$(CC) $(OPTIONS) -pedantic-errors -c -o $@ $^

progress.o: progress.c
	$(CC) $(OPTIONS) -pedantic-errors -c -o $@ $^

main: main.c roofline.o utils.o progress.o stats.o MSC.o
	$(CC) $(DEFINES) $(OPTIONS) -pedantic-errors $^ -o $@ $(HWLOC_FLAG) -lm -ldl $(OMP_FLAG)

benchmark_bandwidth: benchmark_bandwidth.c utils.o roofline.o stats.o progress.o MSC.o
	$(CC) $(OPTIONS) -pedantic-errors $(DEFINES) -o $@ $^ $(HWLOC_FLAG) -lm -ldl $(OMP_FLAG)

clean:
	rm -f *.o *.s main benchmark_bandwidth

