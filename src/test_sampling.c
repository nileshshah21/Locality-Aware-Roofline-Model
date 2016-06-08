#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "sampling.h"

#define SIZE 1000

double a [SIZE][SIZE];
double b [SIZE][SIZE];
double c [SIZE][SIZE];

static void matrix_mul(){
    unsigned i,j,k;
    for(i=0;i<SIZE;i++)
	for(j=0;j<SIZE;j++)
	    for(k=0;k<SIZE;k++)
		c[i][j] += a[i][k]*b[k][j];
}

int main(){
    unsigned i;
    int eventset;
    struct roofline_sample out;
    long long flops = SIZE * SIZE * SIZE * 2;
    for(i=0;i<SIZE; i++){
	memset(a[i], 1, SIZE*sizeof(double));
	memset(b[i], 1, SIZE*sizeof(double));
	memset(c[i], 1, SIZE*sizeof(double));
    }

    roofline_sampling_init(NULL);
    roofline_eventset_init(&eventset);
    roofline_sampling_start(eventset, &out);
    matrix_mul();
    roofline_sampling_stop(eventset, &out);
    roofline_eventset_destroy(&eventset);
    roofline_sample_print(&out , "test");
    printf("Theoric flops = %lld\n", flops);
    roofline_sampling_fini();
    return EXIT_SUCCESS;
}


