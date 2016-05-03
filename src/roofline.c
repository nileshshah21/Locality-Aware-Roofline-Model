#include "roofline.h"
#include "MSC/MSC.h"
#ifdef USE_OMP
#include <omp.h>
#endif

hwloc_topology_t topology = NULL;            /* Current machine topology  */
size_t           alignement = 0;             /* Level 1 cache line size */
size_t           LLC_size = 0;               /* Last Level Cache size */
float            cpu_freq = 0;               /* In Hz */
char *           compiler = NULL;            /* The compiler name to compile the roofline validation. */
hwloc_obj_t      first_node = NULL;          /* The first node where to bind threads */
unsigned         n_threads = 1;              /* The number of threads for benchmark */
struct roofline_progress_bar progress_bar;   /* Global progress bar of the benchmark */
char             vendor[13];                 /* Processor vendor */
unsigned int     model = 1920;               /* Processor model (initialized with bit 7:4 set to one) */
unsigned int     family = 30720;             /* Processor family (initialized with bit 11:8 set to one) */


static void roofline_get_cpuid_info(){
    unsigned int index = 0;
    unsigned int regs[4];

    __asm__ __volatile__("CPUID\n\t" : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3]): "a"(index)); 
    snprintf(vendor, sizeof(vendor), "%c%c%c%c%c%c%c%c%c%c%c%c", 
	     ((char*)regs)[4], ((char*)regs)[5], ((char*)regs)[6], ((char*)regs)[7],
	     ((char*)regs)[12], ((char*)regs)[13], ((char*)regs)[14], ((char*)regs)[15],
	     ((char*)regs)[8], ((char*)regs)[9], ((char*)regs)[10], ((char*)regs)[11]);
    
    index = 1;
    __asm__ __volatile__("CPUID\n\t" : "=a"(regs[0]): "a"(index));
    model = model & regs[0];
    family = family & regs[0];
}


#ifdef USE_OMP
int roofline_lib_init(int with_hyperthreading)
#else
    int roofline_lib_init(__attribute__ ((unused)) int with_hyperthreading)
#endif
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

    /* Get first node and number of threads */
    first_node = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NODE, 0);
#ifdef USE_OMP
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
    roofline_get_cpuid_info();
    printf("%s, model:0x%x, family=:0x%x\n", vendor, model, family);
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

void roofline_fpeak(FILE * output)
{
    char info[128];
    struct roofline_sample_out result;
    struct roofline_sample_in in = {1,NULL,0};  
    double sd = 0;

    snprintf(info, sizeof(info), "%5s","FLOPS");
    roofline_output_clear(&result);
    roofline_autoset_loop_repeat(fpeak_bench, &in, BENCHMARK_MIN_DUR, 10000);
    sd = roofline_repeat_bench(fpeak_bench,&in,&result, roofline_output_median);    
#ifdef USE_OMP
    roofline_print_sample(output, first_node, &result, sd, info);
#else
    roofline_print_sample(output, hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, 0), &result, sd, info);
#endif
}

size_t resize_splitable_chunk(size_t size, int overflow){
    size_t ret, modulo;
    modulo = chunk_size*n_threads;
    if(size%modulo == 0)
	return size;
    ret = size - size%modulo;
    if(overflow)
	ret+=overflow;
    return ret;
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
    hwloc_obj_t child;

    /* set legend to append to results */
    memset(info,0,sizeof(info));
    memset(progress_info,0,sizeof(progress_info));
    snprintf(info, sizeof(info),"%5s", roofline_type_str(type));

    nc = hwloc_obj_type_snprintf(progress_info,sizeof(progress_info),memory, 0);
    nc += snprintf(progress_info+nc, sizeof(progress_info)-nc, ":%d %s", memory->logical_index, info);

    /* various initializations */
    bench = roofline_oi_bench(oi,type);

    /* bind memory */
    roofline_hwloc_membind(memory);

    /* Set lower bound size as 4 times under memories size to be sure it won't hold in lower memories whatever the number of threads */
    child  = roofline_hwloc_get_previous_memory(memory);
    if(child == NULL)
	lower_bound_size = chunk_size*n_threads;
    else{
	lower_bound_size = 8*roofline_hwloc_get_memory_size(child);
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
	    fprintf(stderr, "%u*chunk_size(%lu B) greater memory %s(%lu B).\n",
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
	in.stream_size = resize_splitable_chunk(sizes[s],0);
	roofline_autoset_loop_repeat(bench, &in, BENCHMARK_MIN_DUR,4);
	roofline_output_clear(&(samples[s]));
	sd = roofline_repeat_bench(bench, &in, &(samples[s]), roofline_output_median);
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
    roofline_memory(output,mem,0,type);
}

inline void roofline_oi(FILE * output, hwloc_obj_t mem, int type, double oi){
    roofline_memory(output,mem,oi,type);
}
