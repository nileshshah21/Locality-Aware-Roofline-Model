#include "roofline.h"
#include "MSC/MSC.h"
#ifdef USE_OMP
#include <omp.h>
#endif

#define BENCHMARK_N_SAMPLE 32
#define OUTPUT stdout

void (* bandwidth_bench) (struct roofline_sample_in * in, struct roofline_sample_out * out);    

struct roofline_sample_out empty_out(){
    struct roofline_sample_out output = {0,0,0,0,0};
    return output;
}

static void run_bench(hwloc_obj_t memory){
    roofline_hwloc_membind(memory);
#ifdef USE_OMP
    omp_set_num_threads(hwloc_get_nbobjs_inside_cpuset_by_type(topology,memory->cpuset, HWLOC_OBJ_CORE));
#endif
    /* Set the bounds size */
    hwloc_obj_t prev = roofline_hwloc_get_previous_memory(memory);
    while(roofline_hwloc_get_memory_size(prev) >= roofline_hwloc_get_memory_size(memory))
	prev = roofline_hwloc_get_previous_memory(memory);
    size_t min = roofline_MAX(1024,4*roofline_hwloc_get_memory_size(prev));
    if(memory->type == HWLOC_OBJ_NODE)
	min *= 4;
    size_t max = roofline_MIN(LLC_size*64, roofline_hwloc_get_memory_size(memory));
    struct roofline_sample_in  input = {1,NULL, 0};
    struct roofline_sample_out output;
    alloc_chunk_aligned(&input.stream, max);

    int i, n_sizes = BENCHMARK_N_SAMPLE;
    size_t * sizes = roofline_log_array(min, max, &n_sizes);
    double sd;

    for(i=0;i<n_sizes;i++){ 
	output = empty_out();
	input.stream_size = alloc_chunk_aligned(NULL,sizes[i]);
	roofline_autoset_loop_repeat(bandwidth_bench, &input, BENCHMARK_MIN_DUR);
	sd = roofline_repeat_bench(bandwidth_bench, &input, &output, roofline_output_median);
	roofline_mkstr_stack(info,128); 
	snprintf(info,sizeof(info),"%10zu", sizes[i]);
	roofline_print_sample(OUTPUT, memory, &output, sd, info);
    }

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

    roofline_mkstr_stack(info,128); sprintf(info, "%10s", "size");
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
