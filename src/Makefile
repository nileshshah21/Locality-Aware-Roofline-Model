#############################################################################################
#                                      Configuration                                        #
#############################################################################################
#Duration time of each sample (millisecond)
LAST=10
#Type of parallelisme among {SEQ, OMP}
OPENMP=yes
#Machine specific code implementing benchmarks: MSC/MSC.h {intel only for now}
MSC=intel
#Use PAPI and compile tool for code sampling
PAPI=yes

#############################################################################################
#                                              BUILD                                        #
#############################################################################################
CC=gcc
OMP_FLAG=-fopenmp
LDFLAGS=-lhwloc -ldl
CFLAGS=-g -Wall -Werror -Wextra
SRC=roofline.c utils.c stats.c progress.c
OBJ=$(patsubst %.c, %.o, $(SRC)) MSC.o
PREFIX=$(HOME)/local

.PHONY: all install uninstall clean


ifeq (yes, $(PAPI))
all: roofline librfsample.so
else
all: roofline
endif

ifeq (yes, $(OPENMP))
CFLAGS+=-DUSE_OMP
LDFLAGS+=$(OMP_FLAG)
endif

%.o: %.c
	$(CC) -O8 -march=native -DBENCHMARK_MIN_DUR=$(LAST) $(CFLAGS) -c -o $@ $^ $(LDFLAGS)

MSC.o: MSC/$(MSC).c
	$(CC) -O8 -march=native -DCC=$(CC) $(CFLAGS) -c -o $@ $^ $(LDFLAGS)

roofline: main.c $(OBJ)
	$(CC) $(CFLAGS) -pedantic-errors $^ -o $@ $(LDFLAGS) -lhwloc -lm

librfsample.so: sampling.c
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@ -lpapi -fopenmp

test: test_sampling.c libroofline.so
	$(CC) $(CFLAGS) $< -o $@ -lroofline
	./$@

install:
	cp sampling.h $(PREFIX)/include/
	cp librsampling.so $(PREFIX)/lib/
	cp roofline $(PREFIX)/bin/

uninstall:
	rm $(PREFIX)/include/sampling.h $(PREFIX)/lib/librsampling.so $(PREFIX)/bin/roofline

clean:
	rm -f *.o *.s *.so roofline

