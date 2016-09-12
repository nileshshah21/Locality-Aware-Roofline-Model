#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <papi.h>

#include "sampling.h"

static unsigned BYTES = 8;
static FILE *   output_file = NULL;  
 unsigned n_threads = 1;

struct roofline_sample{
  /* All sample type specific data */
  int type;
  int eventset;        /* Eventset of this sample used to read events */
  long long values[4]; /* Values to be store on counter read */
  long nanoseconds;    /* Timestamp in cycles where the roofline started */
  long flops;          /* The amount of flops computed */
  long bytes;          /* The amount of bytes of type transfered */
};

static char * roofline_cat_info(const char * info){
  size_t len = 2;
  char * env_info = getenv("LARM_INFO");
  char * ret = NULL;
  if(info != NULL) len += strlen(info);
  if(env_info != NULL) len += strlen(env_info);
  ret = malloc(len);
  memset(ret, 0 ,len);
  if(info != NULL && env_info != NULL)snprintf(ret, len+2, "%s_%s", info, env_info);
  else if(info != NULL)snprintf(ret, len, "%s", info);
  else if(env_info != NULL)snprintf(ret, len, "%s", env_info);
  return ret;
}

static void
PAPI_handle_error(const int err)
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


void roofline_sample_clear(struct roofline_sample * out){
  if(out->eventset!=PAPI_NULL){
    PAPI_call_check(PAPI_reset(out->eventset), PAPI_OK, "Eventset reset failed\n");
  }
  out->nanoseconds = 0;
  out->flops = 0;
  out->bytes = 0;
}

struct roofline_sample * new_roofline_sample(int type){
  struct roofline_sample * s = malloc(sizeof(struct roofline_sample));
  if(s==NULL){perror("malloc"); return NULL;}
  s->nanoseconds = 0;
  s->flops = 0;
  s->bytes = 0;
  s->eventset = PAPI_NULL;
  s->type = type;
  
#ifdef _OPENMP
#pragma omp critical
  {
#endif
    PAPI_call_check(PAPI_create_eventset(&(s->eventset)), PAPI_OK, "PAPI eventset initialization failed\n");
    PAPI_call_check(PAPI_add_named_event(s->eventset, "FP_ARITH:SCALAR_DOUBLE"), PAPI_OK, "Failed to add FP_ARITH:SCALAR_DOUBLE event\n");
    PAPI_call_check(PAPI_add_named_event(s->eventset, "FP_ARITH:128B_PACKED_DOUBLE"), PAPI_OK, "Failed to add FP_ARITH:128B_PACKED_DOUBLE event\n");
    PAPI_call_check(PAPI_add_named_event(s->eventset, "FP_ARITH:256B_PACKED_DOUBLE"), PAPI_OK, "Failed to add FP_ARITH:256B_PACKED_DOUBLE event\n");
    switch(type){
    case TYPE_STORE:
      PAPI_call_check(PAPI_add_named_event(s->eventset, "MEM_UOPS_RETIRED:ALL_STORES"), PAPI_OK, "Failed to add MEM_UOPS_RETIRED:ALL_STORES event\n");
      break;
    default:
      PAPI_call_check(PAPI_add_named_event(s->eventset, "MEM_UOPS_RETIRED:ALL_LOADS"), PAPI_OK, "Failed to add MEM_UOPS_RETIRED:ALL_LOADS event\n");
      break;
    } 
#ifdef _OPENMP
  }
#endif
  return s;
}

void delete_roofline_sample(struct roofline_sample * s){
  PAPI_destroy_eventset(&(s->eventset));
  free(s);
}

void roofline_sample_print(struct roofline_sample * s , const char * info)
{
  char * info_cat = roofline_cat_info(info);
  switch(s->type) {
  case TYPE_STORE:
    fprintf(output_file, "%16ld %16ld %16ld %10d %10s %s\n",
	    s->nanoseconds, s->bytes, s->flops, n_threads, "store", info_cat);
    break;
  default:
    fprintf(output_file, "%16ld %16ld %16ld %10d %10s %s\n",
	    s->nanoseconds, s->bytes, s->flops, n_threads, "load", info_cat);
    break;
  }
}

static inline void roofline_print_header(){
  fprintf(output_file, "%16s %16s %16s %10s %10s %s\n",
	  "Nanoseconds", "Bytes", "Flops", "n_threads", "type", "info");
}

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
  int eax = 0, ebx = 0, ecx = 0, edx = 0;
    
  /* Check SSE */
  eax = 1;
  __asm__ __volatile__ ("CPUID\n\t": "=c" (ecx), "=d" (edx): "a" (eax));
  if ((edx & 1 << 25) || (edx & 1 << 26)) {BYTES = 16; printf("SSE support found\n");}

  /* Check AVX */
  if ((ecx & 1 << 28) || (edx & 1 << 26)) {BYTES = 32; printf("AVX support found\n");}
  eax = 7; ecx = 0;
  __asm__ __volatile__ ("CPUID\n\t": "=b" (ebx), "=c" (ecx): "a" (eax), "c" (ecx));
  if ((ebx & 1 << 5)) {BYTES = 32; printf("AVX2 support found\n");}

  /* AVX512 foundation. Not checked */
  if ((ebx & 1 << 16)) {BYTES = 64; printf("AVX512 support found\n");}
    
  PAPI_call_check(PAPI_library_init(PAPI_VER_CURRENT), PAPI_VER_CURRENT, "PAPI version mismatch\n");
  PAPI_call_check(PAPI_is_initialized(), PAPI_LOW_LEVEL_INITED, "PAPI initialization failed\n");
  printf("PAPI library initialization done\n");

  printf("library initialization done\n");  
  roofline_print_header(output_file, "info");
}

void roofline_sampling_fini(){
  fclose(output_file);
}


void roofline_sampling_start(struct roofline_sample * out){
  struct timespec t;
  PAPI_start(out->eventset);
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
  out->nanoseconds = t.tv_nsec + 1e9*t.tv_sec;
}


void roofline_sampling_stop(struct roofline_sample * out){
  struct timespec t;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
  PAPI_stop(out->eventset,out->values);
  out->nanoseconds = (t.tv_nsec + 1e9*t.tv_sec) - out->nanoseconds;
  out->flops = out->values[0] + 2 * out->values[1] + 3 * out->values[2];
  out->bytes  = out->values[3]*BYTES;
}


#ifdef _OPENMP
#include <omp.h>
struct roofline_sample shared;

void roofline_sample_accumulate(struct roofline_sample * out, struct roofline_sample * with){
  out->nanoseconds = out->nanoseconds>with->nanoseconds ? out->nanoseconds : with->nanoseconds;
  out->bytes      += with->bytes;
  out->flops      += with->flops;
}
#endif

