#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <omp.h>
#include <papi.h>
#include "sampling.h"

const unsigned BYTES=8;
unsigned FLOPS = 1;

#define ROOFLINE_LOAD     1   /* benchmark type */
#define ROOFLINE_STORE    4   /* benchmark type */
const char * roofline_type_str(int type){
    switch(type){
    case ROOFLINE_LOAD:
	return "load";
	break;
    case ROOFLINE_STORE:
	return "store";
	break;
    default:
	return "";
	break;
    }
}


#define roofline_rdtsc(c_high,c_low) __asm__ __volatile__ ("CPUID\n\tRDTSC\n\tmovq %%rdx, %0\n\tmovq %%rax, %1\n\t" :"=r" (c_high), "=r" (c_low)::"%rax", "%rbx", "%rcx", "%rdx")
#define roofline_rdtsc_diff(high, low) ((high << 32) | low)

FILE *   output_file = NULL;  
uint64_t freq = 0;
unsigned n_threads = 1;

static inline void roofline_print_header(){
    fprintf(output_file, "%10s %10s %10s %10s %10s %10s %s\n",
	    "Throughput", "GByte/s", "GFlop/s", "Flops/Byte", "n_threads", "type", "info");
}

static char * roofline_cat_info(const char * info){
    size_t len = 0;
    char * env_info = getenv("LARM_INFO");
    char * ret = NULL;
    if(info != NULL){len += strlen(info);}
    if(env_info != NULL){len += strlen(env_info);}
    ret = malloc(len+2);
    memset(ret, 0 ,len+2);
    if(info != NULL && env_info != NULL){snprintf(ret, len+2, "%s_%s", info, env_info);}
    else if(info != NULL){snprintf(ret, len, "%s", info);}
    else if(env_info != NULL){snprintf(ret, len, "%s", env_info);}
    return ret;
}

static void roofline_sample_print_type(float throughput, float bandwidth, float perf, float oi, int type, const char * info)
{
    fprintf(output_file, "%10f %10f %10f %10f %10d %10s %s\n",
	    throughput, bandwidth, perf, oi, n_threads, roofline_type_str(type), info);
}

void roofline_sample_print(struct roofline_sample * out , const char * info)
{
    char * info_cat = roofline_cat_info(info);
    long cyc = out->ts_end - out->ts_start;
    float perf = (float)(out->flops * freq) / (float)(1e9*cyc);
    float throughput, bandwidth, oi;
    if(out->store_ins>0){
	throughput = (float)(out->flop_ins+out->store_ins) / (float)cyc;
	bandwidth = (float)(out->store_bytes*freq) / (float)(1e9*cyc);
	oi = (float)out->flops / (float)out->store_bytes;
	roofline_sample_print_type(throughput, bandwidth, perf, oi, ROOFLINE_STORE, info_cat);
    }
    if(out->load_ins>0){
	throughput = (float)(out->flop_ins+out->load_ins) / (float)cyc;
	bandwidth = (float)(out->load_bytes*freq) / (float)(1e9*cyc);
	oi = (float)out->flops / (float)out->load_bytes;
	roofline_sample_print_type(throughput, bandwidth, perf, oi, ROOFLINE_LOAD, info_cat);
    }
 
    free(info_cat);
    fflush(output_file);
}

static void
PAPI_handle_error(int err)
{
    if(err!=0)
	fprintf(stderr,"PAPI error %d: ",err);
    switch(err){
    case PAPI_EINVAL:
	fprintf(stderr,"Invalid argument.\n");
	break;
    case PAPI_ENOINIT:
	fprintf(stderr,"PAPI library is not initialized.\n");
	break;
    case PAPI_ENOMEM:
	fprintf(stderr,"Insufficient memory.\n");
	break;
    case PAPI_EISRUN:
	fprintf(stderr,"Eventset is already_counting events.\n");
	break;
    case PAPI_ECNFLCT:
	fprintf(stderr,"This event cannot be counted simultaneously with another event in the monitor eventset.\n");
	break;
    case PAPI_ENOEVNT:
	fprintf(stderr,"This event is not available on the underlying hardware.\n");
	break;
    case PAPI_ESYS:
	fprintf(stderr, "A system or C library call failed inside PAPI, errno:%s\n",strerror(errno)); 
	break;
    case PAPI_ENOEVST:
	fprintf(stderr,"The EventSet specified does not exist.\n");
	break;
    case PAPI_ECMP:
	fprintf(stderr,"This component does not support the underlying hardware.\n");
	break;
    case PAPI_ENOCMP:
	fprintf(stderr,"Argument is not a valid component. PAPI_ENOCMP\n");
	break;
    case PAPI_EBUG:
	fprintf(stderr,"Internal error, please send mail to the developers.\n");
	break;
    default:
	perror("");
	break;
    }
}

#define PAPI_call_check(call,check_against, ...) do{	\
    int err = call;					\
    if(err!=check_against){				\
	fprintf(stderr, __VA_ARGS__);			\
	PAPI_handle_error(err);				\
	exit(EXIT_FAILURE);				\
    }							\
    } while(0)



void roofline_sampling_init(const char * output){
    printf("roofline sampling library initialization...\n");
    if(output == NULL){
	output_file = stdout;
    }
    else if((output_file = fopen(output,"a+")) == NULL){
	perror("fopen");
	exit(EXIT_FAILURE);
    }

    /* Check whether flops counted will be AVX or SSE */
#if defined (__AVX__)
    FLOPS = 4;
#elif defined (__SSE__)
    FLOPS = 2;
#endif

    /* Check if cpu frequency has been defined */
    char * freq_str = NULL;
#ifndef CPU_FREQ
    freq_str = getenv("CPU_FREQ");
    if(freq_str == NULL){
	fprintf(stderr,"Please define the machine cpu frequency (in Hertz): set environment CPU_FREQ\n");
	exit(EXIT_FAILURE);
    }
    freq = atol(freq_str);
#else 
    freq = CPU_FREQ;
#endif 

    printf("PAPI library initialization...\n");
    PAPI_call_check(PAPI_library_init(PAPI_VER_CURRENT), PAPI_VER_CURRENT, "PAPI version mismatch\n");
    PAPI_call_check(PAPI_is_initialized(), PAPI_LOW_LEVEL_INITED, "PAPI initialization failed\n");

    roofline_print_header(output_file, "info");
}

void roofline_sampling_fini(){
    fclose(output_file);
}


/* instructions, load_instructions, double, double[2], double[4]*/
void roofline_eventset_init(int * eventset){
    *eventset = PAPI_NULL;
    PAPI_call_check(PAPI_create_eventset(eventset), PAPI_OK, "PAPI eventset initialization failed\n");
    PAPI_call_check(PAPI_add_named_event(*eventset, "MEM_UOPS_RETIRED:ALL_STORES"), PAPI_OK, "Failed to add store instructions counter\n");
    PAPI_call_check(PAPI_add_named_event(*eventset, "MEM_UOPS_RETIRED:ALL_LOADS"), PAPI_OK, "Failed to add load instructions counter\n");
    PAPI_call_check(PAPI_add_named_event(*eventset, "FP_ARITH:PACKED"), PAPI_OK, "Failed to count double floating point instructions counter\n");
    PAPI_call_check(PAPI_add_named_event(*eventset, "FP_ARITH:SCALAR_DOUBLE"), PAPI_OK, "Failed to count double floating point instructions counter\n"); 
}

void roofline_eventset_destroy(int * eventset){
    PAPI_destroy_eventset(eventset);
}

void roofline_sampling_start(int eventset, struct roofline_sample * out){
    uint64_t c_high, c_low;
    roofline_rdtsc(c_high, c_low);
    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
    PAPI_start(eventset);
}


void roofline_sampling_stop(int eventset, struct roofline_sample * out){
    uint64_t c_high, c_low;
    long long values[5];
    PAPI_stop(eventset,values);
    roofline_rdtsc(c_high, c_low);
    out->ts_end       = roofline_rdtsc_diff(c_high, c_low);
    out->flop_ins     = values[2]+values[3];
    out->flops        = values[2]*FLOPS+values[3];
    out->load_ins     = values[1];
    out->load_bytes   = values[1]*BYTES;
    out->store_ins    = values[0];
    out->store_bytes  = values[0]*BYTES;
}


#ifdef _OPENMP
struct roofline_sample shared;

void roofline_sample_clear(struct roofline_sample * out){
    out->ts_start = 0;
    out->ts_end = 0;
    out->flop_ins = 0;
    out->flops = 0;
    out->load_ins = 0;
    out->load_bytes = 0;
    out->store_ins = 0;
    out->store_bytes = 0;
}

void roofline_sample_accumulate(struct roofline_sample * out, struct roofline_sample * with){
    out->ts_end       += with->ts_end - with->ts_start;
    out->load_ins     += with->load_ins;
    out->load_bytes   += with->load_bytes;
    out->store_ins    += with->store_ins;
    out->store_bytes  += with->store_bytes;
    out->flops        += with->flops;
    out->flop_ins     += with->flop_ins;
}
#endif
