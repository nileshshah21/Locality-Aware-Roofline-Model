#############################################################################################
#                                      Configuration                                        #
#############################################################################################
#Duration time of each sample (millisecond)
LAST=10
#Machine specific code implementing benchmarks: MSC/MSC.h {intel only for now}
MSC=intel
#Use PAPI and compile tool for code sampling
PAPI=yes

#############################################################################################
#                                              BUILD                                        #
#############################################################################################
CC=gcc
OMP_FLAG= #-fopenmp
LDFLAGS=-lhwloc -ldl $(OMP_FLAG)
CFLAGS=-g -Wall -Werror -Wextra -DCC=$(CC) -DOMP_FLAG=$(OMP_FLAG)
SRC=roofline.c utils.c stats.c progress.c
OBJ=$(patsubst %.c, %.o, $(SRC)) MSC.o
PREFIX=$(HOME)/local
BIN=roofline
LIB=librfsampling.so

.PHONY: all install uninstall clean


ifeq (yes, $(PAPI))
all: $(BIN) $(LIB)
else
all: $(BIN)
endif

%.o: %.c
	$(CC) -march=native -DBENCHMARK_MIN_DUR=$(LAST) $(CFLAGS) -c -o $@ $^ $(LDFLAGS)

MSC.o: MSC/$(MSC).c
	$(CC) -march=native -Wl,-rpath=$(PWD) -DCC=$(CC) $(CFLAGS) -c -o $@ $^ $(LDFLAGS)

$(BIN): main.c $(OBJ)
	$(CC) $(CFLAGS) -pedantic-errors $^ -o $@ $(LDFLAGS) -lhwloc -lm

$(LIB): sampling.c
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@ -lpapi

test: test_sampling.c $(LIB)
	$(CC) $(CFLAGS) $< -o $@ -lrfsampling
	./$@

install:
	cp sampling.h $(PREFIX)/include/
	cp $(LIB) $(PREFIX)/lib/
	cp $(BIN) $(PREFIX)/bin/

uninstall:
	rm $(PREFIX)/include/sampling.h $(PREFIX)/lib/$(LIB) $(PREFIX)/bin/$(BIN)

clean:
	rm -f *.o *.s *.so roofline

