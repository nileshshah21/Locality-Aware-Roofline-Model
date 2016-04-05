#include "roofline.h"
#include "MSC/MSC.h"
#ifdef USE_OMP
#include <omp.h>
#endif

hwloc_topology_t topology = NULL;            /* Current machine topology  */
size_t           alignement = 0;             /* Level 1 cache line size */
size_t           LLC_size = 0;               /* Last Level Cache size */
float            cpu_freq = 0;               /* In Hz */
char *           compiler = NULL;              /* The compiler name to compile the roofline validation. */
struct roofline_progress_bar progress_bar;   /* Global progress bar of the benchmark */

int roofline_lib_init(void)
{
    hwloc_obj_t L1, LLC;
    char * cpu_freq_str;

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

    /* Bind to PU:0 */
    roofline_hwloc_cpubind(hwloc_get_obj_by_type(topology,HWLOC_OBJ_PU,0)->cpuset);

    /* get first cache linesize */
    L1 = roofline_hwloc_get_next_memory(NULL);
    if(L1==NULL){
	errEXIT("No cache found.");
    }
    if(!roofline_hwloc_objtype_is_cache(L1->type) || 
       (L1->attr->cache.type != HWLOC_OBJ_CACHE_UNIFIED && 
	L1->attr->cache.type != HWLOC_OBJ_CACHE_DATA)){
	errEXIT("First memory obj is not a data cache.");
    }

    /* Set alignement */
    alignement = L1->attr->cache.linesize;

    /* Find LLC cache size */
    LLC = hwloc_get_root_obj(topology);
    while(LLC != NULL && !roofline_hwloc_objtype_is_cache(LLC->type))
	LLC = LLC->first_child;
    if(LLC == NULL)
	errEXIT("Error: no LLC cache found\n");
    LLC_size = ((struct hwloc_cache_attr_s *)LLC->attr)->size*4;

    /* Check if cpu frequency has been defined */
#ifndef BENCHMARK_CPU_FREQ
    cpu_freq_str = getenv("BENCHMARK_CPU_FREQ");
    if(cpu_freq_str == NULL)
	errEXIT("Please define the machine cpu frequency (in Hertz) with build option BENCHMARK_CPU_FREQ or the environement variable of the same name");
    cpu_freq = atof(cpu_freq_str);
#else 
    cpu_freq = BENCHMARK_CPU_FREQ
#endif 

    /* set compiler */
#ifdef CC
    compiler = roofline_macro_xstr(CC);
#else
    compiler = getenv("CC");
    if(compiler==NULL){
	errEXIT("Undefined compiler. Please set env CC to your compiler.");
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

void roofline_output_accumulate(struct roofline_sample_out * a, struct roofline_sample_out * b){
#ifdef USE_OMP
#pragma omp critical
    {
#endif
    uint64_t cyc_a = a->ts_end-a->ts_start;
    uint64_t cyc_b = b->ts_end-b->ts_start;
    a->ts_end = roofline_MAX(cyc_a, cyc_b);
    a->flops = a->flops + b->flops;
    a->bytes = a->bytes + b->bytes;
    a->instructions = a->instructions + b->instructions;
#ifdef USE_OMP
}
#endif
}

inline void print_roofline_sample_output(struct roofline_sample_out * out){
    printf("%20lu %20lu %20lu %20lu %20lu\n", 
	   out->ts_start, out->ts_end, out->instructions, out->bytes, out->flops);
}

void roofline_fpeak(FILE * output)
{
    char info[128];
    hwloc_obj_t obj;
    unsigned n_threads;
    struct roofline_sample_out result;
    struct roofline_sample_in in = {1,NULL,0};  
    double sd = 0;

    snprintf(info, sizeof(info), "%5s","FLOPS");
    roofline_output_clear(&result);
    
#ifdef USE_OMP
    n_threads = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
    omp_set_num_threads(n_threads);
    obj = hwloc_get_root_obj(topology);    
#else
    n_threads = 1;
    obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, 0);
#endif

    roofline_autoset_loop_repeat(fpeak_bench, &in, BENCHMARK_MIN_DUR, 10000);
    sd = roofline_repeat_bench(fpeak_bench,&in,&result, roofline_output_median);    
    roofline_print_sample(output, obj, &result, sd, n_threads, info);
}

static void roofline_memory(FILE * output, hwloc_obj_t memory, double oi, int type){
    char info[128];
    char progress_info[128];
    void (* bench) (struct roofline_sample_in * in, struct roofline_sample_out * out);    
    int nc;
    int s, n_sizes;
    size_t * sizes, lower_bound_size, upper_bound_size;    
    struct roofline_sample_in in;
    struct roofline_sample_out * samples, median;
    double sd;
    unsigned n_threads;
    hwloc_obj_t child;

    /* set legend to append to results */
    memset(info,0,sizeof(info));
    memset(progress_info,0,sizeof(progress_info));
    if(type==ROOFLINE_LOAD)  snprintf(info, sizeof(info),"%5s", "LOAD");
    if(type==ROOFLINE_STORE) snprintf(info, sizeof(info),"%5s", "STORE");
    nc = hwloc_obj_type_snprintf(progress_info,sizeof(progress_info),memory, 0);
    nc += snprintf(progress_info+nc, sizeof(progress_info)-nc, ":%d %s", memory->logical_index, info);

    /* various initializations */
    bench = roofline_oi_bench(oi,type);
    n_threads=1;

#ifdef USE_OMP
    /* Set the number of running threads to the number of Cores sharing the memory */
    n_threads =  hwloc_get_nbobjs_inside_cpuset_by_type(topology,memory->cpuset, HWLOC_OBJ_CORE);
    omp_set_num_threads(n_threads);
#endif    
    
    /* bind memory */
    roofline_hwloc_membind(memory);

    /* Set lower bound size as 4 times under memories size to be sure it won't hold in lower memories whatever the number of threads */
    child  = roofline_hwloc_get_previous_memory(memory);
    if(child != NULL)
	lower_bound_size = 4*roofline_hwloc_get_memory_size(child);
    else
	lower_bound_size = 1024;

    /* Set upper bound size as memory size or 16 times LLC_size */
    upper_bound_size = roofline_hwloc_get_memory_size(memory)/n_threads;
    upper_bound_size = roofline_MAX(upper_bound_size,lower_bound_size);
    upper_bound_size = roofline_MIN(upper_bound_size,LLC_size*16);

    /* get array of input sizes */
    n_sizes  = ROOFLINE_N_SAMPLES;
    sizes = roofline_log_array(lower_bound_size, upper_bound_size, &n_sizes);
    /*Initialize input stream */
    alloc_chunk_aligned(&(in.stream), n_threads*alloc_chunk_aligned(NULL,sizes[n_sizes-1]));
    roofline_alloc(samples, sizeof(*samples)*n_sizes);

    for(s=0;s<n_sizes;s++){
	roofline_progress_set(&progress_bar, "",0,s,n_sizes);
	in.stream_size = n_threads*alloc_chunk_aligned(NULL,sizes[s]);
	roofline_autoset_loop_repeat(bench, &in, BENCHMARK_MIN_DUR,4);
	roofline_output_clear(&(samples[s]));
	roofline_repeat_bench(bench, &in, &(samples[s]), roofline_output_median);
    }

    roofline_progress_set(&progress_bar, "",0,s,n_sizes);    
    median = samples[roofline_output_median(samples,n_sizes)];
    sd = roofline_output_sd(samples, n_sizes);
    roofline_progress_clean();    
    roofline_print_sample(output, memory, &median, sd, n_threads, info);    
    
    /* Cleanup */
    free(samples);
    free(in.stream);
}

void roofline_bandwidth(FILE * output, hwloc_obj_t mem, int type){
    roofline_memory(output,mem,0,type);
}

inline void roofline_oi(FILE * output, hwloc_obj_t mem, int type, double oi){
    roofline_memory(output,mem,oi,type);
}
