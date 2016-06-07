#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "../roofline.h"
#include "MSC.h"

#define ELEM_SIZE sizeof(ROOFLINE_STREAM_TYPE)
#define VEC_LEN 8
#define VEC_BYTES (VEC_LEN * ELEM_SIZE)
#define VEC_FLOPS 16

#define scalar_add(a,b) (a + b)
#define scalar_mul(a,b) (a * b)
#define scalar_triad(a,b,c) (a+c*b)
#define scalar_copy(dst, src) dst = src

#define vec_add(a,b)				\
    scalar_copy(a[0], scalar_add(a[0],b[0]));	\
    scalar_copy(a[1], scalar_add(a[1],b[1]));	\
    scalar_copy(a[2], scalar_add(a[2],b[2]));	\
    scalar_copy(a[3], scalar_add(a[3],b[3]));	\
    scalar_copy(a[4], scalar_add(a[4],b[4]));	\
    scalar_copy(a[5], scalar_add(a[5],b[5]));	\
    scalar_copy(a[6], scalar_add(a[6],b[6]));	\
    scalar_copy(a[7], scalar_add(a[7],b[7]))

#define vec_mul(a,b)				\
    scalar_copy(a[0], scalar_mul(a[0],b[0]));	\
    scalar_copy(a[1], scalar_mul(a[1],b[1]));	\
    scalar_copy(a[2], scalar_mul(a[2],b[2]));	\
    scalar_copy(a[3], scalar_mul(a[3],b[3]));	\
    scalar_copy(a[4], scalar_mul(a[4],b[4]));	\
    scalar_copy(a[5], scalar_mul(a[5],b[5]));	\
    scalar_copy(a[6], scalar_mul(a[6],b[6]));	\
    scalar_copy(a[7], scalar_mul(a[7],b[7]))

#define vec_triad(a,b,c)					\
    scalar_copy(a[0], scalar_triad((a)[0],(b)[0],(c)[0]));	\
    scalar_copy(a[1], scalar_triad((a)[1],(b)[1],(c)[1]));	\
    scalar_copy(a[2], scalar_triad((a)[2],(b)[2],(c)[2]));	\
    scalar_copy(a[3], scalar_triad((a)[3],(b)[3],(c)[3]));	\
    scalar_copy(a[4], scalar_triad((a)[4],(b)[4],(c)[4]));	\
    scalar_copy(a[5], scalar_triad((a)[5],(b)[5],(c)[5]));	\
    scalar_copy(a[6], scalar_triad((a)[6],(b)[6],(c)[6]));	\
    scalar_copy(a[7], scalar_triad((a)[7],(b)[7],(c)[7]))

#define vec_copy(dst,src)			\
    scalar_copy((dst)[0],(src)[0]);		\
    scalar_copy((dst)[1],(src)[1]);		\
    scalar_copy((dst)[2],(src)[2]);		\
    scalar_copy((dst)[3],(src)[3]);		\
    scalar_copy((dst)[4],(src)[4]);		\
    scalar_copy((dst)[5],(src)[5]);		\
    scalar_copy((dst)[6],(src)[6]);		\
    scalar_copy((dst)[7],(src)[7])

size_t alloc_chunk_aligned(ROOFLINE_STREAM_TYPE ** data, size_t size){
    size-=size%VEC_BYTES;
    size+=VEC_BYTES;
    int err;
    if(data != NULL){
	err = posix_memalign((void**)data, alignement, size);
	switch(err){
	case 0:
	    break;
	case EINVAL:
	    fprintf(stderr,"The alignment argument was not a power of two, or was not a multiple of sizeof(void *).\n");
	    break;
	case ENOMEM:
	    fprintf(stderr,"There was insufficient memory to fulfill the allocation request.\n");
	}
	if(*data == NULL)
	    fprintf(stderr,"Chunk is NULL\n");
	if(err || *data == NULL)
	    errEXIT("");

    	memset(*data,0,size);
    }
    return size;
}

void load_bandwidth_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
    ROOFLINE_STREAM_TYPE local_copy[VEC_LEN];
    memset(local_copy,0,sizeof(local_copy));
    volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
    unsigned i, j, end = in->stream_size/sizeof(*(in->stream));

#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high, c_low);
    for(j=0; j<in->loop_repeat; j++){
	for(i=0; i<end; i+=VEC_LEN){
	    vec_copy(local_copy, &(in->stream[i]));
	}
    }
#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high1, c_low1);
    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
    out->bytes = sizeof(local_copy[0]) * (end+1) * in->loop_repeat;
    out->instructions = out->bytes/VEC_BYTES;
}

void store_bandwidth_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
    ROOFLINE_STREAM_TYPE local_copy[VEC_LEN];
    memset(local_copy,0,sizeof(local_copy));
    uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
    unsigned i, j, end = in->stream_size/sizeof(*(in->stream));

#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high, c_low);
    for(j=0; j<in->loop_repeat; j++){
	for(i=0; i<end; i+=VEC_LEN){
	    vec_copy(&(in->stream[i]), local_copy);
	}
    }
#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high1, c_low1);

    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
    out->bytes = in->stream_size * in->loop_repeat;
    out->instructions = out->bytes/VEC_BYTES;
}

inline void fpeak_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
    unsigned j;
    uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
    ROOFLINE_STREAM_TYPE a[VEC_LEN];
    ROOFLINE_STREAM_TYPE b[VEC_LEN];
    ROOFLINE_STREAM_TYPE c[VEC_LEN];

#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high, c_low);
    for(j=0; j<in->loop_repeat; j++){
	vec_triad(a,b,c);
    }
	
#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high1, c_low1);

    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
    out->instructions = in->loop_repeat;
    out->flops = VEC_FLOPS * in->loop_repeat;
}


unsigned flops_per_mop;
unsigned mops_per_flop;
int oi_type;

void oi_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
    unsigned i,j,k, end = in->stream_size/sizeof(*(in->stream));
    uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
    ROOFLINE_STREAM_TYPE a[VEC_LEN];
    ROOFLINE_STREAM_TYPE b[VEC_LEN];
    ROOFLINE_STREAM_TYPE c[VEC_LEN];
    ROOFLINE_STREAM_TYPE stream0[VEC_LEN];
    ROOFLINE_STREAM_TYPE stream1[VEC_LEN];
    memset(stream0,0,sizeof(stream0));
    memset(stream1,0,sizeof(stream1));
    memset(a,0,sizeof(a));
    memset(b,0,sizeof(b));
    memset(c,0,sizeof(c));

    if(flops_per_mop == 1){
#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high, c_low);
    for(j=0; j<in->loop_repeat; j++){
	for(i=0; i<end; i+=2*VEC_LEN){
	    vec_copy(stream0, &(in->stream[i]));
	    vec_triad(a,b,c);
	    vec_copy(stream1, &(in->stream[2*i]));
	}
    }
#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high1, c_low1);
    out->flops = VEC_FLOPS * end/(VEC_LEN);
    }

    else if(flops_per_mop > 1){
#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high, c_low);
    for(j=0; j<in->loop_repeat; j++){
	for(i=0; i<end; i+=VEC_LEN){
	    vec_copy(stream0, &(in->stream[i]));
	    for(k=0; k<flops_per_mop; k++){
		vec_triad(a,b,c);
	    }
	}
    }
#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high1, c_low1);
    out->flops = VEC_FLOPS * flops_per_mop * end/(VEC_LEN);
    }

    else{
#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high, c_low);
    for(j=0; j<in->loop_repeat; j++){
	for(i=0; i<end; i+=VEC_LEN){
	    vec_copy(stream0, &(in->stream[i]));
	    if(i%mops_per_flop == 0){
		vec_triad(a,b,c);
	    }
	}
    }
#ifdef USE_OMP
#pragma omp barrier
#pragma omp master
#endif
    roofline_rdtsc(c_high1, c_low1);
    out->flops = VEC_FLOPS * end / (VEC_LEN * mops_per_flop);
    }

    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
    out->bytes = in->stream_size * in->loop_repeat;
    out->instructions = out->bytes / VEC_BYTES + out->flops/VEC_FLOPS;
    /* printf("Bandwidth = %f GByte/s\n", out->bytes * cpu_freq * 1e-9 / (out->ts_end - out->ts_start)); */
    /* printf("Flops/Bytes = %f\n", (float)out->flops/(float)out->bytes); */
}


void (* roofline_oi_bench(double oi, int type))(struct roofline_sample_in * in, struct roofline_sample_out * out)
{
    if(oi==0 && type==ROOFLINE_LOAD) return load_bandwidth_bench;
    if(oi==0 && type==ROOFLINE_STORE) return store_bandwidth_bench;

    oi_type = type;
    if(oi>1){
	flops_per_mop = oi/2;
	mops_per_flop = 0;
    }
    else if(oi==1){
	flops_per_mop = 1;
	mops_per_flop = 1;
    }
    else if(oi<1){
	flops_per_mop = 0;
	mops_per_flop = 2/oi;
    }
    return oi_bench;
}

