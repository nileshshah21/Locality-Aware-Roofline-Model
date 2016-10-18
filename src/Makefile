#############################################################################################
#                                      Configuration                                        #
#############################################################################################
#Duration time of each sample (millisecond)
LAST=100
#Number of samples per memory.
N_SAMPLES=16
#Use PAPI and compile tool for code sampling
PAPI=no
# Benchmark for intel plateform
MSC=intel

#############################################################################################
#                                              BUILD                                        #
#############################################################################################
export CC=gcc
export OMP_FLAG=-fopenmp
LDFLAGS=-ldl -lhwloc -lm $(OMP_FLAG)
export CFLAGS= -g -Wall -Werror -Wextra\
	-DROOFLINE_MIN_DUR=$(LAST)\
	-DROOFLINE_N_SAMPLES=$(N_SAMPLES)\
	-DCC=$(CC)\
	-DOMP_FLAG=$(OMP_FLAG)\
#	-DDEBUG2 # -DDEBUG1

SRC=stream.c output.c types.c stats.c topology.c roofline.c
OBJ=$(patsubst %.c, %.o, $(SRC)) MSC/$(MSC)/MSC.a
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
	$(CC) $(CFLAGS) -c -o $@ $^ $(OMP_FLAG)

MSC/$(MSC)/MSC.a: MSC/$(MSC)/*.c
	make -C MSC/$(MSC)

$(BIN): main.c $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(LIB): sampling.c
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@ -lpapi -lrt

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
	make -C MSC/$(MSC) clean

