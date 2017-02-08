#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#ifdef _PAPI_
#include <papi.h>
#endif
#ifdef _OPENMP
#include <omp.h>
#endif
#include <hwloc.h>
#include "sampling.h"
#include "list.h"

static unsigned         BYTES = 8;
static FILE *           output_file = NULL;
static hwloc_topology_t topology;
static list             samples;
static uint64_t *       bindings;

struct roofline_sample{
#ifdef _PAPI_
  int         started;
  int         eventset;    /* Eventset of this sample used to read events */
  long long   values[4];   /* Values to be store on counter read */
#endif
  int         type;        /* TYPE_LOAD or TYPE_STORE */
  long        nanoseconds; /* Timestamp in cycles where the roofline started */
  long        flops;       /* The amount of flops computed */
  long        bytes;       /* The amount of bytes of type transfered */
  hwloc_obj_t location;    /* Where the thread is taking samples */
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

static int roofline_hwloc_obj_snprintf(hwloc_obj_t obj, char * info_in, size_t n){
  int nc;
  memset(info_in,0,n);
  /* Special case for MCDRAM */
  if(obj->type == HWLOC_OBJ_NUMANODE && obj->subtype != NULL && !strcmp(obj->subtype, "MCDRAM"))
    nc = snprintf(info_in, n, "%s", obj->subtype);
  else nc = hwloc_obj_type_snprintf(info_in, n, obj, 0);
  nc += snprintf(info_in+nc,n-nc,":%d",obj->logical_index);
  return nc;
}

static const char * roofline_type_str(int type){
  switch(type){
  case TYPE_LOAD:
    return "load";
    break;
  case TYPE_STORE:
    return "store";
    break;    
  default:
    return "";
    break;    
  }
}

#ifdef _PAPI_
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

void roofline_sampling_eventset_init(hwloc_obj_t PU, int * eventset, int type){
#ifdef _OPENMP
#pragma omp critical
    {
#endif /* _OPENMP */
      
      *eventset = PAPI_NULL;      
      PAPI_call_check(PAPI_create_eventset(eventset), PAPI_OK, "PAPI eventset initialization failed\n");
      /* assign eventset to a component */
      PAPI_call_check(PAPI_assign_eventset_component(*eventset, 0), PAPI_OK, "Failed to assign eventset to commponent: ");
      /* bind eventset to cpu */
      PAPI_option_t cpu_option;
      cpu_option.cpu.eventset=*eventset;
      cpu_option.cpu.cpu_num = PU->os_index;
      PAPI_call_check(PAPI_set_opt(PAPI_CPU_ATTACH,&cpu_option), PAPI_OK, "Failed to bind eventset to cpu: ");
      
      PAPI_call_check(PAPI_add_named_event(*eventset, "FP_ARITH:SCALAR_DOUBLE"), PAPI_OK, "Failed to add FP_ARITH:SCALAR_DOUBLE event\n");
      PAPI_call_check(PAPI_add_named_event(*eventset, "FP_ARITH:128B_PACKED_DOUBLE"), PAPI_OK, "Failed to add FP_ARITH:128B_PACKED_DOUBLE event\n");
      PAPI_call_check(PAPI_add_named_event(*eventset, "FP_ARITH:256B_PACKED_DOUBLE"), PAPI_OK, "Failed to add FP_ARITH:256B_PACKED_DOUBLE event\n");
      switch(type){
      case TYPE_STORE:
	PAPI_call_check(PAPI_add_named_event(*eventset, "MEM_UOPS_RETIRED:ALL_STORES"), PAPI_OK, "Failed to add MEM_UOPS_RETIRED:ALL_STORES event\n");
	break;
      default:
	PAPI_call_check(PAPI_add_named_event(*eventset, "MEM_UOPS_RETIRED:ALL_LOADS"), PAPI_OK, "Failed to add MEM_UOPS_RETIRED:ALL_LOADS event\n");
	break;
      } 
#ifdef _OPENMP
    }
#endif /* _OPENMP */
}

#endif /* _PAPI_ */

static void roofline_sample_reset(struct roofline_sample * s){
  s->nanoseconds = 0;
  s->flops = 0;
  s->bytes = 0;
#ifdef _PAPI_
  PAPI_reset(s->eventset);
#endif /* _PAPI_ */
}

static struct roofline_sample * roofline_sample_accumulate(struct roofline_sample * out,
							   struct roofline_sample * with){
  out->nanoseconds = out->nanoseconds>with->nanoseconds ? out->nanoseconds : with->nanoseconds;
  out->bytes      += with->bytes;
  out->flops      += with->flops;
  return out;
}

static struct roofline_sample * new_roofline_sample(hwloc_obj_t PU, int type){
  struct roofline_sample * s = malloc(sizeof(struct roofline_sample));
  if(s==NULL){perror("malloc"); return NULL;}
  s->nanoseconds = 0;
  s->flops = 0;
  s->bytes = 0;
  s->type = type;
  s->location = PU;
  do{s->location = s->location->parent;} while(s->location && s->location->type!=HWLOC_OBJ_NODE);

#ifdef _PAPI_
  roofline_sampling_eventset_init(PU, &(s->eventset), type);
  s->started = 0;
#endif /* _PAPI_ */
  return s;
}

static void delete_roofline_sample(struct roofline_sample * s){
#ifdef _PAPI_
  PAPI_destroy_eventset(&(s->eventset));
#endif
  free(s);
}

static void roofline_sample_print(struct roofline_sample * s , int n_threads, const char * info)
{
  char location[16]; roofline_hwloc_obj_snprintf(s->location, location, sizeof(location));
  char * info_cat = roofline_cat_info(info);
  
  fprintf(output_file, "%16s %16ld %16ld %16ld %10d %10s %s\n",
  	  location,
  	  s->nanoseconds,
  	  s->bytes,
  	  s->flops,
  	  n_threads,
  	  roofline_type_str(s->type),
  	  info_cat);
}

static inline void roofline_print_header(){
  fprintf(output_file, "%16s %16s %16s %16s %10s %10s %s\n",
	  "Location", "Nanoseconds", "Bytes", "Flops", "n_threads", "type", "info");
}

void roofline_sampling_init(const char * output, int append_output, int type){

  /* Open output */
  int print_header = 0;
  char * mode = "a";
  
  if(output == NULL){
    output_file = stdout;
    print_header = 1;
  } else {
    /* Check file existence */
    if(access( output, F_OK ) == -1 || !append_output ){
      print_header = 1;
      mode = "w+";
    }
    if((output_file = fopen(output, mode)) == NULL){
      perror("fopen");
      exit(EXIT_FAILURE);
    }
  }

  /* Initialize topology */
  if(hwloc_topology_init(&topology) ==-1){
    fprintf(stderr, "hwloc_topology_init failed.\n");
    exit(EXIT_FAILURE);
  }
  hwloc_topology_set_icache_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
  if(hwloc_topology_load(topology) ==-1){
    fprintf(stderr, "hwloc_topology_load failed.\n");
    exit(EXIT_FAILURE);
  }

  /* Initialize bindings array */
  unsigned i, max_threads;
#ifdef _OPENMP
  #pragma omp parallel
  max_threads = omp_get_max_threads();
#else
  max_threads = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
#endif
  bindings = malloc(sizeof(*bindings)*max_threads);
  for(i=0;i<max_threads;i++){bindings[i] = 0;}
		    
#ifdef _PAPI_
  /* Initialize PAPI library */
  PAPI_call_check(PAPI_library_init(PAPI_VER_CURRENT), PAPI_VER_CURRENT, "PAPI version mismatch\n");
  PAPI_call_check(PAPI_is_initialized(), PAPI_LOW_LEVEL_INITED, "PAPI initialization failed\n");
#endif

  /* Create one sample per PU */
  struct roofline_sample * s;
  unsigned n_PU = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
  hwloc_obj_t first_PU, obj = NULL;  
  samples = new_list(sizeof(s), n_PU, (void(*)(void*))delete_roofline_sample);
  for(i=0; i<n_PU; i++){
    obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, i);
    s = new_roofline_sample(obj, type);
    while(obj && obj->type != HWLOC_OBJ_NODE){obj = obj->parent;}
    s->location = obj;
    list_push(samples, s);
  }

  /* Store a sublist of sample in each Node */
  obj=NULL;
  while((obj=hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_NODE, obj)) != NULL){
    if(obj->arity == 0){continue;}
    n_PU = hwloc_get_nbobjs_inside_cpuset_by_type(topology, obj->cpuset, HWLOC_OBJ_PU);
    first_PU = obj; while(first_PU->type!=HWLOC_OBJ_PU){first_PU = first_PU->first_child;}    
    obj->userdata = sub_list(samples, first_PU->logical_index, n_PU);    
  }
   
  /* Check whether flops counted will be AVX or SSE */
  int eax = 0, ebx = 0, ecx = 0, edx = 0;
  /* Check SSE */
  eax = 1;
  __asm__ __volatile__ ("CPUID\n\t": "=c" (ecx), "=d" (edx): "a" (eax));
  if ((edx & 1 << 25) || (edx & 1 << 26)) {BYTES = 16;}
  /* Check AVX */
  if ((ecx & 1 << 28) || (edx & 1 << 26)) {BYTES = 32;}
  eax = 7; ecx = 0;
  __asm__ __volatile__ ("CPUID\n\t": "=b" (ebx), "=c" (ecx): "a" (eax), "c" (ecx));
  if ((ebx & 1 << 5)) {BYTES = 32;}
  /* AVX512. Not checked */
  if ((ebx & 1 << 16)) {BYTES = 64;}


  /* Passed initialization then print header */
  if(print_header){roofline_print_header(output_file, "info");}
}

void roofline_sampling_fini(){
  hwloc_obj_t Node = NULL;
  while((Node=hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_NODE, Node)) != NULL){
    if(Node->arity == 0){continue;}
    delete_list(Node->userdata);
  }
  free(bindings);
  delete_list(samples);  
  hwloc_topology_destroy(topology);
  fclose(output_file);
}

static void is_thread(struct roofline_sample * s, unsigned * n_threads){
  *n_threads += (s->nanoseconds>0?1:0);
}

static void roofline_samples_reduce(list l, const char * info){
  unsigned n_threads = 0;
  struct roofline_sample *s;
  
  list_apply(l, is_thread, (&n_threads));
  s = list_reduce(l, (void*(*)(void*,void*))roofline_sample_accumulate);
  roofline_sample_print(s, n_threads, info);
}

static struct roofline_sample * roofline_sampling_caller(int id){
  if(id > 0){return list_get(samples, bindings[id]);}
  long cpu = -1;

  /* Check if calling thread is bound on a PU */
  hwloc_cpuset_t binding = hwloc_bitmap_alloc();
  if(hwloc_get_cpubind(topology, binding, HWLOC_CPUBIND_THREAD) == -1){perror("get_cpubind");}
  else if(hwloc_bitmap_weight(binding) == 1){cpu = hwloc_bitmap_first(binding);}
  hwloc_bitmap_free(binding);
  
  if(cpu == -1){
    if((cpu = sched_getcpu()) == -1){perror("getcpu");}
  }

  if(cpu != -1){
    hwloc_obj_t PU = hwloc_get_pu_obj_by_os_index(topology, (int)cpu);
    bindings[-id] = PU->logical_index;
    return list_get(samples, PU->logical_index);
  }

  return NULL;
}

#ifdef _PAPI_
static void * roofline_sequential_sampling_start(__attribute__ ((unused)) long flops,__attribute__ ((unused)) long bytes)
#else
static void * roofline_sequential_sampling_start(long flops, long bytes)
#endif
{
  struct timespec t;
  int id = 0;
#ifdef _OPENMP
  id = omp_get_thread_num();
#endif
  struct roofline_sample * s = roofline_sampling_caller(-id);
  if(s != NULL){
    roofline_sample_reset(s);
#pragma omp barrier        
#ifdef _PAPI_
    s->started = 1;    
    PAPI_start(s->eventset);
#else
    s->bytes = bytes;
    s->flops = flops;
#endif
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    s->nanoseconds = t.tv_nsec + 1e9*t.tv_sec;
  }
  return (void*)s;
}

#ifdef _OPENMP
void * roofline_sampling_start(int parallel, long flops, long bytes)
#else
  void * roofline_sampling_start(__attribute__ ((unused)) int parallel, long flops, long bytes)
#endif
{
  struct roofline_sample * ret = NULL;
#ifdef _OPENMP
  if(parallel && !omp_in_parallel()){
#pragma omp parallel
    {
      roofline_sequential_sampling_start(flops, bytes);
    }
  } else {
    ret = roofline_sequential_sampling_start(flops, bytes);
  }
#else
  ret = roofline_sequential_sampling_start(flops, bytes);
#endif
  return ret;
}


static void roofline_sequential_sampling_stop(void *sample, const char* info){
  if(sample == NULL){return;}
  struct roofline_sample * s = (struct roofline_sample*)sample;
  struct timespec t;  
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
#ifdef _PAPI_
  if(s->started){PAPI_stop(s->eventset, s->values);}
  s->flops = s->values[0] + 2 * s->values[1] + 4 * s->values[2];
  long fp_op = s->values[0] + s->values[1] + s->values[2];
  if(fp_op > 0){
    s->bytes = 8*s->values[3]*(4*s->values[2] + 2*s->values[1] + s->values[0])/fp_op;
  } else {
    s->bytes = 8*s->values[3];
  }
#endif
  s->nanoseconds = (t.tv_nsec + 1e9*t.tv_sec) - s->nanoseconds;
#ifdef _OPENMP
#pragma omp barrier
#pragma omp single
  {
#endif
    hwloc_obj_t Node = NULL;
    while((Node=hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_NODE, Node)) != NULL){
      if(Node->arity == 0){continue;}
      roofline_samples_reduce(Node->userdata, info);
    }
    list_apply(samples, roofline_sample_reset);
#ifdef _OPENMP    
  }
#endif
}

void roofline_sampling_stop(void *sample, const char* info){
#ifdef _OPENMP
  if(sample == NULL){
    if(!omp_in_parallel()){
#pragma omp parallel
      roofline_sequential_sampling_stop(roofline_sampling_caller(omp_get_thread_num()), info);
    } else {
      roofline_sequential_sampling_stop(roofline_sampling_caller(omp_get_thread_num()), info);
    }
  } else {
    roofline_sequential_sampling_stop(sample, info);
  }
#else
  roofline_sequential_sampling_stop(sample, info);
#endif
}

