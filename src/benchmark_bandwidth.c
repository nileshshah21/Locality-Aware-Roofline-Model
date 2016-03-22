#include "roofline.h"
#include "MSC/MSC.h"
#ifdef USE_OMP
#include <omp.h>
#endif

#define BENCHMARK_N_SAMPLE 32
#define OUTPUT stdout

char info[128];
void (* bandwidth_bench) (struct roofline_sample_in * in, struct roofline_sample_out * out);    

static void run_bench(hwloc_obj_t memory){
    unsigned n_child;
    size_t min, max;
    struct roofline_sample_in  input;
    struct roofline_sample_out output;
    int i, n_sizes;
    size_t * sizes;
    double sd;
    unsigned n_threads, tid;
    hwloc_obj_t child;
    
    roofline_hwloc_membind(memory);

#ifdef USE_OMP
    n_threads = hwloc_get_nbobjs_inside_cpuset_by_type(topology,memory->cpuset, HWLOC_OBJ_CORE);
#else
    tid = 1; n_threads = 1;
#endif

    /* Set lower bound size as 4 times the sum of under memories size */
    child  = roofline_hwloc_get_previous_memory(memory);
    if(child != NULL){
	n_child = hwloc_get_nbobjs_inside_cpuset_by_depth(topology, memory->cpuset, child->depth);
	min = 4*n_child*roofline_hwloc_get_memory_size(child);
    }
    else
	min = 1024;
    max = roofline_MIN(LLC_size*16, roofline_hwloc_get_memory_size(memory));


    alloc_chunk_aligned(&input.stream, max);
    n_sizes = BENCHMARK_N_SAMPLE;
    sizes = roofline_log_array(min, max, &n_sizes);
#ifdef USE_OMP
    for(tid=1; tid<=n_threads; tid++){ 
	omp_set_num_threads(tid);
#endif
	for(i=0;i<n_sizes;i++){ 
	    roofline_output_clear(&output);
	    input.stream_size = alloc_chunk_aligned(NULL,sizes[i]);
	    roofline_autoset_loop_repeat(bandwidth_bench, &input, BENCHMARK_MIN_DUR);
	    sd = roofline_repeat_bench(bandwidth_bench, &input, &output, roofline_output_median);
	    memset(info, 0, sizeof(info));
	    snprintf(info,sizeof(info),"%10zu", sizes[i]);
	    roofline_print_sample(OUTPUT, memory, &output, sd, tid, info);
	}
#ifdef USE_OMP
    }
#endif
    free(input.stream);
    free(sizes);
}

void usage(char * argv0){
    printf("%s -h -m <hwloc_ibj:idx>\n", argv0);
    printf("%s --help --memory <hwloc_obj:idx>\n", argv0);
    exit(EXIT_SUCCESS);
}

int main(int argc, char ** argv){
    hwloc_obj_t mem = NULL;
    roofline_lib_init();
    bandwidth_bench = roofline_oi_bench(0,ROOFLINE_LOAD);
    int i = 0;
    while(++i < argc){
	if(!strcmp(argv[i],"--help") || !strcmp(argv[i],"-h"))
	    usage(argv[0]);
	else if(!strcmp(argv[i],"--memory") || !strcmp(argv[i],"-m")){
	    mem = roofline_hwloc_parse_obj(argv[++i]);
	}
	else
	    usage(argv[0]);
    }

    memset(info, 0, sizeof(info));
    roofline_print_header(OUTPUT, info);
    
    if(mem == NULL){
	while((mem = roofline_hwloc_get_next_memory(mem)) != NULL){
	    run_bench(mem);
	}
    }
    else
	run_bench(mem);

    roofline_lib_finalize();
    return EXIT_SUCCESS;
}
