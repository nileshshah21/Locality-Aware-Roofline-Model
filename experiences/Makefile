CC=gcc
OMP_FLAG=-fopenmp
LDFLAGS=-lrfsampling
CFLAGS=-Wall -Wextra -Werror

run: main.c kernels.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(OMP_FLAG)

kernels.o: kernels.c
	$(CC) -mtune=native -march=native -O3 -c $< -o $@ $(OMP_FLAG)

clean:
	rm -f run kernels.o


