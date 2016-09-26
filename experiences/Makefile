CC=gcc
OMP_FLAG=-fopenmp
LDFLAGS=-lm -lrfsampling -lgslcblas #-L/opt-x86_64/intel/mkl/lib/intel64/ -lmkl_rt -lpthread -lm -ldl 
CFLAGS=-g

all: run

KERNELS_C= ddot.c triad.c scale.c
KERNELS_O= $(patsubst %.c,%.o,$(KERNELS_C))

%.o: %.c
	$(CC) $(CFLAGS) -mtune=native -march=native -O2 -c $< -o $@ $(OMP_FLAG)

%.s: %.c
	$(CC) $(CFLAGS) -mtune=native -march=native -O2 -S $< -o $@ $(OMP_FLAG)

run: main.c $(KERNELS_O)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(OMP_FLAG)

clean:
	rm -f run *.o *.s


