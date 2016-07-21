#include "roofline.h"
#include "MSC/MSC.h"
#if defined(_OPENMP)
#include <omp.h>
#endif

hwloc_topology_t topology = NULL;            /* Current machine topology  */
size_t           alignement = 0;             /* Level 1 cache line size */
size_t           LLC_size = 0;               /* Last Level Cache size */
float            cpu_freq = 0;               /* In Hz */
char *           compiler = NULL;            /* The compiler name to compile the roofline validation. */
char *           omp_flag = NULL;            /* The openmp flag to compile the roofline validation. */
hwloc_obj_t      first_node = NULL;          /* The first node where to bind threads */
unsigned         n_threads = 1;              /* The number of threads for benchmark */
int              per_thread = 0;             /* Should results be printed with per thread value */
unsigned int     roofline_types;             /* What rooflines do we want in byte array */
struct roofline_progress_bar progress_bar;   /* Global progress bar of the benchmark */


#if defined(_OPENMP)
int roofline_lib_init(int with_hyperthreading)
#else
    int roofline_lib_init(__attribute__ ((unused)) int with_hyperthreading)
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

    /* Get first node and number of threads */
    first_node = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NODE, 0);
#if defined(_OPENMP)
    if(with_hyperthreading)
	n_threads = hwloc_get_nbobjs_inside_cpuset_by_type(topology, first_node->cpuset, HWLOC_OBJ_PU);
    else
	n_threads = hwloc_get_nbobjs_inside_cpuset_by_type(topology, first_node->cpuset, HWLOC_OBJ_CORE);	
    omp_set_num_threads(n_threads);
#endif

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
    LLC_size = ((struct hwloc_cache_attr_s *)LLC->attr)->size;

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
    compiler = roofline_stringify(CC);
#else
    compiler = getenv("CC");
    if(compiler==NULL){
	errEXIT("Undefined compiler. Please set env CC to your compiler.");
    }
#endif
#ifdef OMP_FLAG
    omp_flag = roofline_stringify(OMP_FLAG);
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
    char info[128];
    struct roofline_sample_out result;
    struct roofline_sample_in in = {1,NULL,0};  
    double sd = 0;

    snprintf(info, sizeof(info), "%5s",roofline_type_str(type));
    roofline_output_clear(&result);
    roofline_autoset_loop_repeat(fpeak_benchmark, &in, type, BENCHMARK_MIN_DUR, 10000);
    sd = roofline_repeat_bench(fpeak_benchmark,&in,&result, type, roofline_output_median);    
#if defined(_OPENMP)
    roofline_print_sample(output, first_node, &result, sd, info);
#else
    roofline_print_sample(output, hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, 0), &result, sd, info);
#endif
}

extern size_t chunk_size;
static size_t resize_splitable_chunk(size_t size){
    int nthreads = 1;
#if defined(_OPENMP)
#pragma omp parallel
#pragma omp single
    nthreads = omp_get_num_threads();
#endif
    if(size%(chunk_size*nthreads) == 0){return size;}
    else{return (chunk_size*nthreads)*(1+(size/(chunk_size*nthreads)));}
}

static size_t roofline_memalign(double ** data, size_t size){
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


static void roofline_memory(FILE * output, hwloc_obj_t memory, int type,
			    void(* bench)(const struct roofline_sample_in *, struct roofline_sample_out *, int))
{
    char info[128];
    char progress_info[128];
    int nc;
    int s, n_sizes;
    size_t * sizes, lower_bound_size, upper_bound_size;    
    struct roofline_sample_in in;
    struct roofline_sample_out * samples, median;
    double sd;
    hwloc_obj_t child;

    /* set legend to append to results */
    memset(info,0,sizeof(info));
    memset(progress_info,0,sizeof(progress_info));
    snprintf(info, sizeof(info),"%5s", roofline_type_str(type));

    nc = hwloc_obj_type_snprintf(progress_info,sizeof(progress_info),memory, 0);
    nc += snprintf(progress_info+nc, sizeof(progress_info)-nc, ":%d %s", memory->logical_index, info);

    /* bind memory */
    roofline_hwloc_membind(memory);

    /* Set lower bound size as 4 times under memories size to be sure it won't hold in lower memories whatever the number of threads */
    child  = roofline_hwloc_get_previous_memory(memory);
    if(child == NULL)
	lower_bound_size = chunk_size*n_threads;
    else{
	lower_bound_size = 4*roofline_hwloc_get_memory_size(child);
	if(n_threads > 1){
	    lower_bound_size = lower_bound_size*n_threads/hwloc_bitmap_weight(child->cpuset);
	}
    }

    /* Set upper bound size as memory size or 16 times LLC_size */
    upper_bound_size = roofline_hwloc_get_memory_size(memory);
    upper_bound_size = roofline_MIN(upper_bound_size,LLC_size*32);
    if(n_threads > 1){
	if(child!=NULL)
	    upper_bound_size = upper_bound_size*n_threads/hwloc_bitmap_weight(child->cpuset);
	else
	    upper_bound_size = upper_bound_size*n_threads/hwloc_bitmap_weight(memory->cpuset);
    }

    if(upper_bound_size<lower_bound_size){
	if(child!=NULL){
	fprintf(stderr, "%s(%f MB) above %s(%f MB) is not large enough to be split into 4*%u\n", 
		hwloc_type_name(memory->type), roofline_hwloc_get_memory_size(memory)/1e6, 
		hwloc_type_name(child->type), roofline_hwloc_get_memory_size(child)/1e6, 
		n_threads/hwloc_bitmap_weight(child->cpuset));
	}
	else{
	    fprintf(stderr, "minimum chunk size(%u*%lu B) greater than memory %s size(%lu B). Skipping.\n",
		    n_threads, chunk_size, 
		    hwloc_type_name(memory->type), roofline_hwloc_get_memory_size(memory));
	}
	return;
    }

    /* get array of input sizes */
    n_sizes  = ROOFLINE_N_SAMPLES;
    sizes = roofline_log_array(lower_bound_size, upper_bound_size, &n_sizes);
    if(sizes==NULL)
	return;
    /*Initialize input stream */
    roofline_memalign(&(in.stream), upper_bound_size);
    roofline_alloc(samples, sizeof(*samples)*n_sizes);

    for(s=0;s<n_sizes;s++){
	roofline_progress_set(&progress_bar, "",0,s,n_sizes);
	in.stream_size = resize_splitable_chunk(sizes[s]);
	if(in.stream_size > upper_bound_size){break;}
	roofline_autoset_loop_repeat(bench, &in, type, BENCHMARK_MIN_DUR,4);
	roofline_output_clear(&(samples[s]));
	sd = roofline_repeat_bench(bench, &in, &(samples[s]), type, roofline_output_median);
	/* roofline_print_sample(output, memory, &(samples[s]), sd, info);     */
    }

    roofline_progress_set(&progress_bar, "",0,s,n_sizes);
    median = samples[roofline_output_median(samples,n_sizes)];
    sd = roofline_output_sd(samples, n_sizes);
    roofline_progress_clean();    
    roofline_print_sample(output, memory, &median, sd, info);    
    
    /* Cleanup */
    free(samples);
    free(in.stream);
}

void roofline_bandwidth(FILE * output, hwloc_obj_t mem, int type){
    if(type & ROOFLINE_LOAD){roofline_memory(output,mem,ROOFLINE_LOAD,bandwidth_benchmark);}
    if(type & ROOFLINE_LOAD_NT){roofline_memory(output,mem,ROOFLINE_LOAD_NT,bandwidth_benchmark);}
    if(type & ROOFLINE_STORE){roofline_memory(output,mem,ROOFLINE_STORE,bandwidth_benchmark);}
    if(type & ROOFLINE_STORE_NT){roofline_memory(output,mem,ROOFLINE_STORE_NT,bandwidth_benchmark);}
}

void roofline_flops(FILE * output, int type){
   if(type & ROOFLINE_ADD){roofline_fpeak(output,ROOFLINE_ADD);}
   if(type & ROOFLINE_MUL){roofline_fpeak(output,ROOFLINE_MUL);}
   if(type & ROOFLINE_MAD){roofline_fpeak(output,ROOFLINE_MAD);}
}

void roofline_oi(FILE * output, hwloc_obj_t mem, int type, double oi){
    void(* bench)(const struct roofline_sample_in *, struct roofline_sample_out *, int);
    int i, tp;
    const int mem_types[4] = {ROOFLINE_LOAD, ROOFLINE_LOAD_NT, ROOFLINE_STORE, ROOFLINE_STORE_NT};
    for(i=0;i<4;i++){
	tp = mem_types[i];
	if(type & tp){
	    bench = roofline_oi_bench(oi,tp);
	    if(bench == NULL){continue;}
	    roofline_memory(output,mem,tp,bench);
	}
    }
}

