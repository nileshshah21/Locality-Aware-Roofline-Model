#include <math.h>
#include "roofline.h"
#include "topology.h"
#include "stream.h"
#include "output.h"
#include "utils.h"
#include "stats.h"
#include "MSC/MSC.h"

hwloc_topology_t topology = NULL;            /* Current machine topology  */
size_t           alignement = 0;             /* Level 1 cache line size */
size_t           max_size = 0;               /* Last Level Cache size */
float            cpu_freq = 0;               /* In Hz */
unsigned         n_threads = 1;              /* The number of threads for benchmark */
unsigned int     roofline_types;             /* What rooflines do we want in byte array */
hwloc_obj_t      root;                       /* The root of topology to select the amount of threads */
off_t            L1_size;                    /* size of L1_cache */
static hwloc_obj_type_t leaf_type;

#if defined(_OPENMP)
int roofline_lib_init(hwloc_topology_t topo, int with_hyperthreading, int whole_system)
#else
  int roofline_lib_init(hwloc_topology_t topo, __attribute__ ((unused)) int with_hyperthreading, int whole_system)
#endif
{
  hwloc_obj_t L1, LLC;
  char * cpu_freq_str;

  /* Check hwloc version */
  roofline_hwloc_check_version();

  /* Initialize topology */
  if(topo != NULL) hwloc_topology_dup(&topology, topo);
  else{
    if(hwloc_topology_init(&topology) ==-1){
      fprintf(stderr, "hwloc_topology_init failed.\n");
      return -1;
    }
    hwloc_topology_set_icache_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
    if(hwloc_topology_load(topology) ==-1){
      fprintf(stderr, "hwloc_topology_load failed.\n");
      return -1;
    }
  }
    
  /* Get first node and number of threads */
  if(!whole_system) root = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NODE, 0);
  else root = hwloc_get_root_obj(topology);

  /* bind future threads to root */
  leaf_type = with_hyperthreading ? HWLOC_OBJ_PU:HWLOC_OBJ_CORE;
  
#if defined(_OPENMP)
  if(with_hyperthreading)
    n_threads = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, HWLOC_OBJ_PU);
  else
    n_threads = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, HWLOC_OBJ_CORE);	
  omp_set_num_threads(n_threads);
#endif
  
  /* get first cache linesize */
  L1 = roofline_hwloc_get_next_memory(NULL, whole_system);
  if(L1==NULL) ERR_EXIT("No cache found.");
  if(!roofline_hwloc_objtype_is_cache(L1->type) || 
     (L1->attr->cache.type != HWLOC_OBJ_CACHE_UNIFIED && 
      L1->attr->cache.type != HWLOC_OBJ_CACHE_DATA)){
    ERR_EXIT("First memory obj is not a data cache.");
  }
  alignement = L1->attr->cache.linesize;
  L1_size = roofline_hwloc_get_memory_size(L1);
  
  /* Find LLC cache size to set maximum buffer size */ 
  LLC = hwloc_get_root_obj(topology);
  while(LLC != NULL && !roofline_hwloc_objtype_is_cache(LLC->type)) LLC = LLC->first_child;
  if(LLC == NULL) ERR_EXIT("Error: no LLC cache found\n");
  max_size = (((struct hwloc_cache_attr_s *)LLC->attr)->size)*128;

  /* Check if cpu frequency has been defined */
#ifndef CPU_FREQ
  cpu_freq_str = getenv("CPU_FREQ");
  if(cpu_freq_str == NULL)
    ERR_EXIT("Please define the machine cpu frequency (in Hertz) with build option CPU_FREQ or the environement variable of the same name");
  cpu_freq = atof(cpu_freq_str);
#else 
  cpu_freq = CPU_FREQ;
#endif 

  return 0;
}

inline void roofline_lib_finalize(void)
{
  hwloc_topology_destroy(topology);
}


void roofline_fpeak(FILE * output, int op_type)
{
  int i;
  roofline_output out;
  long repeat = roofline_autoset_repeat(NULL, NULL, op_type, NULL);
  
#ifdef _OPENMP
#pragma omp declare reduction(+ : roofline_output : roofline_output_accumulate(&omp_out,&omp_in))
#endif
  for(i=0; i<ROOFLINE_N_SAMPLES; i++){
    roofline_output_clear(&out);
    
#ifdef _OPENMP
#pragma omp parallel reduction(+:out)
    {
      roofline_hwloc_cpubind(leaf_type);
#endif
      benchmark_fpeak(op_type, &out, repeat);

#if defined(_OPENMP)
    }
    roofline_output_print(output, root, &out, op_type);
#else

    roofline_output_print(output, hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, 0), &out, op_type);
#endif
  }
}

static void roofline_memory(FILE * output, const hwloc_obj_t memory, const int op_type, void * benchmark)
{
  int i, n_sizes;
  size_t * sizes, low_size, up_size;
  roofline_output out;
  roofline_stream src = NULL, dst = NULL;
  long repeat;
  unsigned tid = 0;
  void (*  benchmark_function)(roofline_stream, roofline_output *, int, long) = benchmark;
  
  /* check memory is above root */
  if(memory->depth < root->depth){
    roofline_debug2("Skip memory level above top desired memory %s\n", hwloc_type_name(root->type));
    return;
  }
  
  /* Get memory size bounds */
  if(roofline_hwloc_get_memory_bounds(memory, &low_size, &up_size, op_type) == -1) return;
  
  /* get input sizes */
  n_sizes = ROOFLINE_N_SAMPLES;
  sizes = roofline_log_array(low_size, up_size, &n_sizes);
  if(sizes==NULL){
    roofline_debug2("Computing array of input sizes from %lu to %lu failed\n", low_size, up_size);
    return;
  }

  /*Initialize input stream */
  src = new_roofline_stream(up_size, op_type);
  dst = new_roofline_stream(up_size, op_type);
  
  /* bind memory */
#ifdef _OPENMP
#pragma omp parallel firstprivate(tid)
  {
    tid = omp_get_thread_num();
#endif
    roofline_hwloc_cpubind(leaf_type);
    struct roofline_stream_s src_chunk, dst_chunk;
    roofline_stream_split(src, &(src_chunk), n_threads, tid, op_type);
    roofline_stream_split(dst, &(dst_chunk), n_threads, tid, op_type);
    /* bind memory */
    roofline_hwloc_set_area_membind(memory, src_chunk.stream, src_chunk.alloc_size);
    roofline_hwloc_set_area_membind(memory, dst_chunk.stream, dst_chunk.alloc_size);
#ifdef _OPENMP
  }
#endif
  
  for(i=0;i<n_sizes;i++){
    roofline_stream_set_size(src, sizes[i], op_type);
    if(op_type == ROOFLINE_COPY) roofline_stream_set_size(dst, sizes[i], op_type);
    repeat = roofline_autoset_repeat(dst, src, op_type, benchmark);
    roofline_output_clear(&out);
    roofline_debug2("size = %luB\n", src->size);
    
#ifdef _OPENMP
#pragma omp declare reduction(+ : roofline_output : roofline_output_accumulate(&omp_out,&omp_in))
#pragma omp parallel private(tid) reduction(+:out)
    {
      tid = omp_get_thread_num();
#endif
      roofline_hwloc_cpubind(leaf_type);
      struct roofline_stream_s src_chunk, dst_chunk;
      roofline_stream_split(src, &(src_chunk), n_threads, tid, op_type);
      roofline_stream_split(dst, &(dst_chunk), n_threads, tid, op_type);
  
      if(op_type == ROOFLINE_COPY)
      {
        benchmark_double_stream(&(dst_chunk), &(src_chunk), &out, op_type, repeat);
      }
      else if(op_type == ROOFLINE_LOAD    ||
	      op_type == ROOFLINE_LOAD_NT ||
	      op_type == ROOFLINE_STORE   ||
	      op_type == ROOFLINE_STORE_NT||
	      op_type == ROOFLINE_2LD1ST)
      {
	benchmark_single_stream(&(src_chunk), &out, op_type, repeat);
      }
      else if(benchmark != NULL)
      {
	benchmark_function(&(src_chunk), &out, op_type, repeat);
      }
#ifdef _OPENMP
    }
#endif
    roofline_output_print(output, memory, &out, op_type);
  }

  /* Cleanup */
  delete_roofline_stream(src);
  delete_roofline_stream(dst);
}


void roofline_bandwidth(FILE * output, const hwloc_obj_t mem, const int type){
  char mem_str[128]; memset(mem_str, 0, sizeof(mem_str));
  roofline_hwloc_obj_snprintf(mem,mem_str,sizeof(mem_str));
  
  int restrict_type = roofline_filter_types(mem, type);
  if(restrict_type & ROOFLINE_LOAD){
    printf("benchmark %s memory level for %-4s operation\n", mem_str, roofline_type_str(ROOFLINE_LOAD));
    roofline_memory(output,mem,ROOFLINE_LOAD, NULL);
  }
  if(restrict_type & ROOFLINE_LOAD_NT){
    printf("benchmark %s memory level for %-4s operation\n", mem_str, roofline_type_str(ROOFLINE_LOAD_NT));
    roofline_memory(output,mem,ROOFLINE_LOAD_NT, NULL);
  }
  if(restrict_type & ROOFLINE_STORE){
    printf("benchmark %s memory level for %-4s operation\n", mem_str, roofline_type_str(ROOFLINE_STORE));
    roofline_memory(output,mem,ROOFLINE_STORE, NULL);
  }
  if(restrict_type & ROOFLINE_STORE_NT){
    printf("benchmark %s memory level for %-4s operation\n", mem_str, roofline_type_str(ROOFLINE_STORE_NT));
    roofline_memory(output,mem,ROOFLINE_STORE_NT, NULL);
  }
  if(restrict_type & ROOFLINE_2LD1ST){
    printf("benchmark %s memory level for %-4s operation\n", mem_str, roofline_type_str(ROOFLINE_2LD1ST));
    roofline_memory(output,mem,ROOFLINE_2LD1ST, NULL);
  }
  if(restrict_type & ROOFLINE_COPY){
    printf("benchmark %s memory level for %-4s operation\n", mem_str, roofline_type_str(ROOFLINE_COPY));
    roofline_memory(output,mem,ROOFLINE_COPY, NULL);
  }
}

void roofline_flops(FILE * output, const int type){
  hwloc_obj_t core = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, 0);
  int restrict_type = roofline_filter_types(core, type);   
  if(restrict_type & ROOFLINE_ADD){
    printf("benchmark compute unit for %-8s operation\n", roofline_type_str(ROOFLINE_ADD));
    roofline_fpeak(output,ROOFLINE_ADD);
  }
  if(restrict_type & ROOFLINE_MUL){
    printf("benchmark compute unit for %-8s operation\n", roofline_type_str(ROOFLINE_MUL));
    roofline_fpeak(output,ROOFLINE_MUL);
  }
  if(restrict_type & ROOFLINE_MAD){
    printf("benchmark compute unit for %-8s operation\n", roofline_type_str(ROOFLINE_MAD));
    roofline_fpeak(output,ROOFLINE_MAD);
  }
  if(restrict_type & ROOFLINE_FMA){
    printf("benchmark compute unit for %-8s operation\n", roofline_type_str(ROOFLINE_FMA));
    roofline_fpeak(output,ROOFLINE_FMA);
  }
}

void roofline_oi(FILE * output, const hwloc_obj_t mem, const int type, const unsigned flops, const unsigned bytes){
  void * bench;
  int i, j, mem_type, flop_type, t;
  hwloc_obj_t core = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, 0);
  const int mem_types[6] = {ROOFLINE_LOAD, ROOFLINE_LOAD_NT, ROOFLINE_STORE, ROOFLINE_STORE_NT, ROOFLINE_2LD1ST, ROOFLINE_COPY};
  const int flop_types[4] = {ROOFLINE_ADD, ROOFLINE_MUL, ROOFLINE_MAD, ROOFLINE_FMA};
  char mem_str[128]; memset(mem_str, 0, sizeof(mem_str));
  roofline_hwloc_obj_snprintf(mem,mem_str,sizeof(mem_str));

  flop_type = roofline_filter_types(core, type);
  mem_type = roofline_filter_types(mem, type);
     
  for(i=0;i<6;i++){
    if(mem_types[i] & mem_type){
      for(j=3;j>=0;j--){
	if(flop_types[j] & flop_type){
	  printf("validation of %-4s(%uFlops) and %-8s(%uBytes) operations on %s memory level\n",
		 roofline_type_str(flop_types[j]), flops, roofline_type_str(mem_types[i]), bytes, mem_str);
	  t = flop_types[j]|mem_types[i];
	  bench = benchmark_validation(t, flops, bytes);
	  if(bench == NULL){continue;}
	  roofline_memory(output, mem, t, bench);
	  break;
	}
      }
    }
  }
}

size_t * roofline_log_array(size_t start, size_t end, int * n){
  size_t * sizes, size;
  double multiplier, val;
  int i;

  
  if(end<start)
    return NULL;

  if(*n <= 0)
    *n = ROOFLINE_N_SAMPLES;

  roofline_alloc(sizes,sizeof(*sizes)* (*n));
  size = start;    
  multiplier = 1;
  val = (double)start;

  if(start > 0){
    multiplier = pow((double)end/(double)start,1.0/(double)(*n));
    val = ((double)start*multiplier);
  }
  else{
    val = multiplier = pow((double)end,1.0/(double)(*n));
  }

  i=0;
  while(size<=end && i<*n){
    sizes[i] = size;
    val*=multiplier;
    size = (size_t)val;
    i++;
  }

  if(i==0){
    *n=0; free(sizes); return NULL;
  }
  *n=i;
  return sizes;
}

