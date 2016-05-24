#Compilation
CC=gcc
SRC=roofline.c utils.c stats.c progress.c
OBJ=$(patsubst %.c, %.o, $(SRC)) MSC.o
LDFLAGS=-lhwloc -fopenmp -ldl
CFLAGS=-Wall -Werror -Wextra -DBENCHMARK_MIN_DUR=$(DUR) -DUSE_$(PAR) -DCC=$(CC)

.PHONY: all clean

#Use PAPI and compile tool for code sampling
PAPI=yes


ifeq (yes, $(PAPI))
SRC+=sampling.c
LDFLAGS+=-lpapi
CFLAGS+=-fPIC
all: roofline libroofline.so
else
all: roofline
endif

#Duration time of each sample (millisecond)
DUR=10
#Type of parallelisme among {SEQ, OMP}
PAR=OMP
#Machine specific code implementing MSC/MSC.h {intel}
MSC=intel

%.o: %.c
	$(CC) -O8 -march=native $(CFLAGS) -c -o $@ $^ $(LDFLAGS)

MSC.o: MSC/$(MSC).c
	$(CC) -O8 -march=native $(CFLAGS) -c -o $@ $^ $(LDFLAGS)

MSC.s: MSC/$(MSC).c
	$(CC) -O8 -march=native $(CFLAGS) -S -o $@ $^ $(LDFLAGS)

roofline: main.c $(OBJ)
	$(CC) $(CFLAGS) -pedantic-errors $^ -o $@ -lm $(LDFLAGS)

libroofline.so: $(OBJ)
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@ -lm -ldl $(OMP_FLAG) $(HWLOC_FLAG) -lpapi

test: test_sampling.c libroofline.so
	$(CC) $(CFLAGS) $< -o $@ -lroofline
	./$@

clean:
	rm -f *.o *.s *.so roofline

