#include "roofline.h"
#include "MSC/MSC.h"
#if defined(_OPENMP)
#include <omp.h>
#endif

hwloc_topology_t topology = NULL;            /* Current machine topology  */
size_t           alignement = 0;             /* Level 1 cache line size */
size_t           MAX_SIZE = 0;               /* Last Level Cache size */
float            cpu_freq = 0;               /* In Hz */
char *           compiler = NULL;            /* The compiler name to compile the roofline validation. */
char *           omp_flag = NULL;            /* The openmp flag to compile the roofline validation. */
unsigned         n_threads = 1;              /* The number of threads for benchmark */
unsigned int     roofline_types;             /* What rooflines do we want in byte array */
hwloc_obj_t      root;                       /* The root of topology to select the amount of threads */
struct roofline_progress_bar progress_bar;   /* Global progress bar of the benchmark */


#if defined(_OPENMP)
int roofline_lib_init(hwloc_topology_t topo, int with_hyperthreading, int whole_system)
#else
  int roofline_lib_init(hwloc_topology_t topo, __attribute__ ((unused)) int with_hyperthreading, int whole_system)
#endif
{
  hwloc_obj_t L1, LLC;
    char * cpu_freq_str;

    /* Check hwloc version */
#if HWLOC_API_VERSION >= 0x0020000
    /* Header uptodate for monitor */
    if(hwloc_get_api_version() < 0x20000){
	fprintf(stderr, "hwloc version mismatch, required version 0x20000 or later, found %#08x\n", hwloc_get_api_version());
	return -1;
    }
#else
    fprintf(stderr, "hwloc version too old, required version 0x20000 or later\n");
    return -1;    
#endif

    if(topo != NULL) hwloc_topology_dup(&topology, topo);
    else{
      /* Initialize topology */
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

    /* bind future threads to first NODE */
    roofline_hwloc_cpubind(root);

#if defined(_OPENMP)
    if(with_hyperthreading)
	n_threads = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, HWLOC_OBJ_PU);
    else
	n_threads = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, HWLOC_OBJ_CORE);	
    omp_set_num_threads(n_threads);
#pragma omp parallel
    roofline_hwloc_cpubind(hwloc_get_obj_by_type(topology, with_hyperthreading?HWLOC_OBJ_PU:HWLOC_OBJ_CORE, omp_get_thread_num()));
#endif

    /* get first cache linesize */
    L1 = roofline_hwloc_get_next_memory(NULL, whole_system);
    if(L1==NULL) errEXIT("No cache found.");
    if(!roofline_hwloc_objtype_is_cache(L1->type) || 
       (L1->attr->cache.type != HWLOC_OBJ_CACHE_UNIFIED && 
	L1->attr->cache.type != HWLOC_OBJ_CACHE_DATA)){
	errEXIT("First memory obj is not a data cache.");
    }
    alignement = L1->attr->cache.linesize;

    /* Find LLC cache size to set maximum buffer size */
    LLC = hwloc_get_root_obj(topology);
    while(LLC != NULL && !roofline_hwloc_objtype_is_cache(LLC->type)) LLC = LLC->first_child;
    if(LLC == NULL) errEXIT("Error: no LLC cache found\n");
    MAX_SIZE = (((struct hwloc_cache_attr_s *)LLC->attr)->size)*64;

    /* Check if cpu frequency has been defined */
#ifndef CPU_FREQ
    cpu_freq_str = getenv("CPU_FREQ");
    if(cpu_freq_str == NULL)
	errEXIT("Please define the machine cpu frequency (in Hertz) with build option CPU_FREQ or the environement variable of the same name");
    cpu_freq = atof(cpu_freq_str);
#else 
    cpu_freq = CPU_FREQ;
#endif 

    /* set compilation options */
#ifdef CC
    compiler = STRINGIFY(CC);
#else
    compiler = getenv("CC");
    if(compiler==NULL){
	errEXIT("Undefined compiler. Please set env CC to your compiler.");
    }
#endif
#ifdef OMP_FLAG
    omp_flag = STRINGIFY(OMP_FLAG);
#else
    omp_flag = getenv("OMP_FLAG");
    if(omp_flag==NULL){
	errEXIT("Undefined omp_flag. Please set env OMP_FLAG to your compiler.");
    }
#endif

    return 0;
}

inline void roofline_lib_finalize(void)
{
    hwloc_topology_destroy(topology);
}

void roofline_output_clear(struct roofline_sample_out * out){
    out->ts_start = 0;
    out->ts_end = 0;
    out->bytes = 0;
    out->flops = 0;
    out->instructions = 0;
}

inline void print_roofline_sample_output(struct roofline_sample_out * out){
    printf("%20lu %20lu %20lu %20lu %20lu\n", 
	   out->ts_start, out->ts_end, out->instructions, out->bytes, out->flops);
}

void roofline_fpeak(FILE * output, int type)
{
    struct roofline_sample_out result;
    struct roofline_sample_in in = {1,NULL,0};  
    int i;
    roofline_autoset_loop_repeat(fpeak_benchmark, &in, type, BENCHMARK_MIN_DUR, 10000);

    for(i=0; i<BENCHMARK_REPEAT; i++){
      roofline_output_clear(&result);
      fpeak_benchmark(&in, &result, type);
#if defined(_OPENMP)
      roofline_print_sample(output, root, &result, type);
#else
      roofline_print_sample(output, hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, 0), &result, type);
#endif
    }
}

static size_t resize_splitable_chunk(size_t size, int type){
    size_t chunk_size = get_chunk_size(type);
    if(size%(chunk_size*n_threads) == 0) return size;
    if(size<chunk_size*n_threads) return 0;
    else{return (chunk_size*n_threads)*(1+size/(chunk_size*n_threads));}
}

static void roofline_memory(FILE * output, hwloc_obj_t memory, int type,
			    void(* bench)(const struct roofline_sample_in *, struct roofline_sample_out *, int))
{
    char progress_info[128];
    int nc;
    int s, n_sizes;
    size_t * sizes, lower_bound_size, upper_bound_size;    
    struct roofline_sample_in in;
    struct roofline_sample_out sample;
    hwloc_obj_t child;
    unsigned n_child = 1, n_memory = 1;

    /* check memory is above root */
    if(memory->depth < root->depth){
      fprintf(stderr, "Skip memory level above top desired memory %s\n", hwloc_type_name(root->type));
      return;
    }
    
    /* set legend to append to results */
    if(output != stdout){
      memset(progress_info,0,sizeof(progress_info));
      nc = hwloc_obj_type_snprintf(progress_info,sizeof(progress_info),memory, 0);
      snprintf(progress_info+nc, sizeof(progress_info)-nc, ":%d %s", memory->logical_index, roofline_type_str(type));
    } else printf("\n");
    
    /* bind memory */
    roofline_hwloc_membind(memory);

    child  = roofline_hwloc_get_under_memory(memory, root->type == HWLOC_OBJ_MACHINE);
    if(child != NULL){
      lower_bound_size = 2*roofline_hwloc_get_memory_size(child);
      n_child = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, child->type);
    }
    else{lower_bound_size = get_chunk_size(type);}
    
    /* Multiply number of child in case it would fit lower memory when splitting among threads */
    if(n_threads > 1) lower_bound_size *= n_child;
    
    /* Set upper bound size as memory size or MAX_SIZE */
    upper_bound_size = roofline_MIN(roofline_hwloc_get_memory_size(memory), MAX_SIZE);
    n_memory = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, memory->type);
    if(n_threads > 1) upper_bound_size *= n_memory;

    if(upper_bound_size<lower_bound_size){
	if(child!=NULL){
	fprintf(stderr, "%s(%ld MB) above %s(%ld MB) is not large enough to be split into %u*%ld\n", 
		hwloc_type_name(memory->type), (unsigned long)(roofline_hwloc_get_memory_size(memory)/1e6), 
		hwloc_type_name(child->type), (unsigned long)(roofline_hwloc_get_memory_size(child)/1e6), 
		n_child, (unsigned long)(roofline_hwloc_get_memory_size(child)/1e6));
	}
	else{
	    fprintf(stderr, "minimum chunk size(%u*%lu B) greater than memory %s size(%lu B). Skipping.\n",
		    n_threads, get_chunk_size(type), 
		    hwloc_type_name(memory->type), roofline_hwloc_get_memory_size(memory));
	}
	return;
    }

    /* get array of input sizes */
    n_sizes  = ROOFLINE_N_SAMPLES;
    sizes = roofline_log_array(lower_bound_size, upper_bound_size, &n_sizes);
    if(sizes==NULL){return;}
    
    /*Initialize input stream */
    roofline_memalign(&(in.stream), upper_bound_size);

    for(s=0;s<n_sizes;s++){
      if(output != stdout){roofline_progress_set(&progress_bar, "",0,s,n_sizes);}
      in.stream_size = resize_splitable_chunk(sizes[s], type);
	if(in.stream_size > upper_bound_size){break;}
	roofline_autoset_loop_repeat(bench, &in, type, BENCHMARK_MIN_DUR,4);
	roofline_output_clear(&sample);
	bench(&in, &sample, type);
        roofline_print_sample(output, memory, &sample, type);
    }

    if(output != stdout){roofline_progress_set(&progress_bar, "",0,s,s);}
    if(output != stdout){roofline_progress_clean();}
    
    /* Cleanup */
    free(in.stream);
}


void roofline_bandwidth(FILE * output, hwloc_obj_t mem, int type){
  type = roofline_filter_types(mem, type);
  if(type & ROOFLINE_LOAD)
    roofline_memory(output,mem,ROOFLINE_LOAD,bandwidth_benchmark);
  if(type & ROOFLINE_LOAD_NT)
    roofline_memory(output,mem,ROOFLINE_LOAD_NT,bandwidth_benchmark);
  if(type & ROOFLINE_STORE)
    roofline_memory(output,mem,ROOFLINE_STORE,bandwidth_benchmark);
  if(type & ROOFLINE_STORE_NT)
    roofline_memory(output,mem,ROOFLINE_STORE_NT,bandwidth_benchmark);
  if(type & ROOFLINE_2LD1ST)
    roofline_memory(output,mem,ROOFLINE_2LD1ST,bandwidth_benchmark);
  if(type & ROOFLINE_COPY)
    roofline_memory(output,mem,ROOFLINE_COPY,bandwidth_benchmark);
}

void roofline_flops(FILE * output, int type){
  hwloc_obj_t core = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, 0);
  type = roofline_filter_types(core, type);   
  if(type & ROOFLINE_ADD){roofline_fpeak(output,ROOFLINE_ADD);}
  if(type & ROOFLINE_MUL){roofline_fpeak(output,ROOFLINE_MUL);}
  if(type & ROOFLINE_MAD){roofline_fpeak(output,ROOFLINE_MAD);}
  if(type & ROOFLINE_FMA){roofline_fpeak(output,ROOFLINE_FMA);}
}

void roofline_oi(FILE * output, hwloc_obj_t mem, int type, double oi){
  void(* bench)(const struct roofline_sample_in *, struct roofline_sample_out *, int);
  int i, j, mem_type, flop_type, t;
  hwloc_obj_t core = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, 0);
  const int mem_types[6] = {ROOFLINE_LOAD, ROOFLINE_LOAD_NT, ROOFLINE_STORE, ROOFLINE_STORE_NT, ROOFLINE_2LD1ST, ROOFLINE_COPY};
  const int flop_types[4] = {ROOFLINE_ADD, ROOFLINE_MUL, ROOFLINE_MAD, ROOFLINE_FMA};

  flop_type = roofline_filter_types(core, type);
  mem_type = roofline_filter_types(mem, type);
     
  for(i=0;i<6;i++){
    if(mem_types[i] & mem_type){
      for(j=3;j>=0;j--){
	if(flop_types[j] & flop_type){
	  t = flop_types[j]|mem_types[i];
	  bench = roofline_oi_bench(oi,t);
	  if(bench == NULL){continue;}
	  roofline_memory(output, mem, t, bench);
	  break;
	}
      }
    }
  }
}

