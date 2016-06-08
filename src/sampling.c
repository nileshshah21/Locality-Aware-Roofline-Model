#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <omp.h>
#include <papi.h>
#include "sampling.h"

#define roofline_rdtsc(c_high,c_low) __asm__ __volatile__ ("CPUID\n\tRDTSC\n\tmovq %%rdx, %0\n\tmovq %%rax, %1\n\t" :"=r" (c_high), "=r" (c_low)::"%rax", "%rbx", "%rcx", "%rdx")
#define roofline_rdtsc_diff(high, low) ((high << 32) | low)

FILE *   output_file = NULL;  
uint64_t freq = 0;
unsigned n_threads = 1;

static inline void roofline_print_header(){
    fprintf(output_file, "%12s %20s %20s %16s %10s %10s %10s %10s %10s %10s %s\n",
	    "Obj", "start", "end", "Instructions", "Throughput", "SDev", "GByte/s", "GFlop/s", "Flops/Byte", "n_threads", "info");
}

static char * roofline_cat_info(const char * info){
    size_t len = 0;
    char * env_info = getenv("LARM_INFO");
    char * ret = NULL;
    if(info != NULL){len += strlen(info);}
    if(env_info != NULL){len += strlen(env_info);}
    ret = malloc(len+2);
    memset(ret, 0 ,len+2);
    if(info != NULL && env_info != NULL){snprintf(ret, len, "%s_%s", info, env_info);}
    else if(info != NULL){snprintf(ret, len, "%s", info);}
    else if(env_info != NULL){snprintf(ret, len, "%s", env_info);}
    return ret;
}

void roofline_sample_print(struct roofline_sample * out , char * info)
{
    long cyc;
    char * info_cat = roofline_cat_info(info);
    cyc = out->ts_end - out->ts_start;
    fprintf(output_file, "%12s %20lu %20lu %16lu %10.6f %10.6f %10.3f %10.3f %10.6f %10u %s\n",
	    "Machine", 
	    out->ts_start, 
	    out->ts_end, 
	    out->instructions, 
	    (float)out->instructions / (float) cyc, 
	    0.0,
	    (float)(out->bytes * freq) / (float)(cyc*1e9), 
	    (float)(out->flops * freq) / (float)(1e9*cyc),
	    (float)(out->flops) / (float)(out->bytes),
	    n_threads, 
	    info_cat);
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

    /* Check if cpu frequency has been defined */
    char * freq_str = NULL;
#ifndef CPU_FREQ
    freq_str = getenv("CPU_FREQ");
    if(freq_str == NULL){
	fprintf(stderr,"Please define the machine cpu frequency (in Hertz): set environment CPU_FREQ");
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
    int err = PAPI_OK;
    PAPI_call_check(PAPI_create_eventset(eventset), PAPI_OK, "PAPI eventset initialization failed\n");
    PAPI_call_check(PAPI_add_named_event(*eventset, "PAPI_TOT_INS"), PAPI_OK, "Failed to find instructions counter\n"); 
    err = PAPI_add_named_event(*eventset, "PAPI_LD_INS");
    if(err != PAPI_OK){
	PAPI_handle_error(err);
	printf("PAPI_LD_INS not available\n");
	err = PAPI_add_named_event(*eventset, "MEM_UOPS_RETIRED:ALL_LOADS");
	if(err != PAPI_OK){
	    PAPI_handle_error(err);
	    printf("MEM_UOPS_RETIRED:ALL_LOADS\n");
	    printf("Cannot count load instructions.\n");
	    exit(EXIT_FAILURE);
	}	
    }
    err = PAPI_add_named_event(*eventset, "FP_COMP_OPS_EXE:SSE_SCALAR_DOUBLE");
    if(err != PAPI_OK){
	err = PAPI_add_named_event(*eventset, "FP_ARITH:SCALAR_DOUBLE");
	if(err != PAPI_OK){
	    PAPI_handle_error(err);
	    printf("flop events not available\n");
	    exit(EXIT_FAILURE);
	}
	PAPI_call_check(PAPI_add_named_event(*eventset, "FP_ARITH:128B_PACKED_DOUBLE"), PAPI_OK, "Failed to find FP_ARITH:128B_PACKED_DOUBLE event\n"); 
	PAPI_call_check(PAPI_add_named_event(*eventset, "FP_ARITH:256B_PACKED_DOUBLE"), PAPI_OK, "Failed to find FP_ARITH:256B_PACKED_DOUBLE event\n"); 
    }
    else{
	PAPI_call_check(PAPI_add_named_event(*eventset, "FP_COMP_OPS_EXE:SSE_FP_PACKED_DOUBLE"), PAPI_OK, "Failed to find FP_COMP_OPS_EXE:SSE_FP_PACKED_DOUBLE event\n"); 
	PAPI_call_check(PAPI_add_named_event(*eventset, "SIMD_FP_256:PACKED_DOUBLE"), PAPI_OK, "Failed to find SIMD_FP_256:PACKED_DOUBLE event\n"); 
    }
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

#if defined (__AVX512__)
#define SIMD_BYTES         64
#elif defined (__AVX__)
#define SIMD_BYTES         32
#elif defined (__SSE__)
#define SIMD_BYTES         16
#endif

void roofline_sampling_stop(int eventset, struct roofline_sample * out){
    uint64_t c_high, c_low;
    long long values[5];
    PAPI_stop(eventset,values);
    roofline_rdtsc(c_high, c_low);
    out->ts_end       = roofline_rdtsc_diff(c_high, c_low);
    out->instructions = values[0];
    out->bytes        = values[1]*SIMD_BYTES;
    out->flops        = values[2]+values[3]*2+values[4]*4;
}

