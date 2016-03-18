OPTIONS=-g -Wall -Werror -Wextra

SIMD=AVX #Vectorisation type among {AVX, SSE, SSE2, DBL}
DUR=1    #Duration time of each sample
PAR=SEQ  #Type of parallelisme among {SEQ, OMP}
MSC=MSC/intel.c
HWLOC=-lhwloc

DEFINES=-DUSE_$(SIMD) -DBENCHMARK_MIN_DUR=$(DUR) -DUSE_$(PAR) 

all: main

MSC.o: $(MSC)
	gcc -O0 $(OPTIONS) $(DEFINES) -c -o $@ $^ -ldl -fopenmp

MSC.s: $(MSC)
	gcc -O0 $(OPTIONS) $(DEFINES) -S -o $@ $^ -ldl -fopenmp

roofline.o: roofline.c
	gcc $(OPTIONS) $(DEFINES) -c -o $@ $^ -fopenmp

utils.o: utils.c
	gcc $(OPTIONS) -c -o $@ $^ -lm $(HWLOC)

stats.o: stats.c
	gcc $(OPTIONS) -c -o $@ $^

progress.o: progress.c
	gcc $(OPTIONS) -c -o $@ $^

main: main.c roofline.o utils.o progress.o stats.o MSC.o
	gcc $(DEFINES) $(OPTIONS) $^ -o $@ $(HWLOC) -lm -ldl -fopenmp

benchmark_bandwidth: benchmark_bandwidth.c utils.o roofline.o stats.o progress.o MSC.o
	gcc $(OPTIONS) $(DEFINES) -o $@ $^ $(HWLOC) -lm -ldl -fopenmp

clean:
	rm -f *.o *.s main benchmark_bandwidth

