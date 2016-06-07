#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdio.h>
#include <stdlib.h>
#include <hwloc.h>

extern hwloc_topology_t topology; /* Current machine topology */
extern float    cpu_freq;         /* In Hz */
extern unsigned n_threads;        /* The number of threads for benchmark */

int  roofline_lib_init(int with_hyperthreading);
void roofline_lib_finalize(void);

/************************************ Benchmark memory and fpu  **********************************/

/***********************************  BENCHMARK INPUT  *******************************************/
#ifndef ROOFLINE_STREAM_TYPE
#define ROOFLINE_STREAM_TYPE double
#endif
struct roofline_sample_in{
    /* All sample type specific data */
    unsigned long loop_repeat;     /* Make the roofline longer if you use an external tool to sample */
    ROOFLINE_STREAM_TYPE * stream; /* The buffer to stream */
    size_t stream_size;            /* The total size to stream */
};

void print_roofline_sample_input(struct roofline_sample_in * in);

/***********************************  BENCHMARK OUTPUT *******************************************/
struct roofline_sample_out{
    /* All sample type specific data */
    uint64_t ts_start;       /* Timestamp in cycles where the roofline started */
    uint64_t ts_end;         /* Timestamp in cycles where the roofline ended */
    uint64_t instructions;   /* The number of instructions */
    uint64_t bytes;          /* The amount of bytes transfered */
    uint64_t flops;          /* The amount of flops computed */
};

void roofline_output_clear(struct roofline_sample_out * out);
void print_roofline_sample_output(struct roofline_sample_out * out);

/***********************************  BENCHMARK FUNCTIONS ****************************************/
#define ROOFLINE_LOAD 0
#define ROOFLINE_STORE 1
#define ROOFLINE_COPY 2
#define ROOFLINE_N_SAMPLES 8

void roofline_fpeak    (FILE * output);
void roofline_bandwidth(FILE * output, hwloc_obj_t memory, int type);
void roofline_oi       (FILE * output, hwloc_obj_t memory, int type, double oi);

/******************************************** Progress Bar ***************************************/
struct roofline_progress_bar{
    char * info;
    size_t begin;
    size_t current;
    size_t end;
};

extern struct roofline_progress_bar progress_bar;
void roofline_progress_set  (struct roofline_progress_bar * bar, char * info, size_t low, size_t curr, size_t up);
void roofline_progress_print(struct roofline_progress_bar * bar);
void roofline_progress_clean(void);

/****************************************    Statistics    ****************************************/
#define BENCHMARK_REPEAT 8
#ifndef BENCHMARK_MIN_DUR
#define BENCHMARK_MIN_DUR 10 /* milliseconds */
#endif

int    comp_roofline_throughput(void * a, void * b);
int    roofline_output_min(struct roofline_sample_out * samples, size_t n);
int    roofline_output_max(struct roofline_sample_out * samples, size_t n);
int    roofline_output_median(struct roofline_sample_out * samples, size_t n);
double roofline_output_sd(struct roofline_sample_out * samples, unsigned n);
double roofline_repeat_bench(void (* bench_fun)(struct roofline_sample_in *, struct roofline_sample_out *), struct roofline_sample_in * in, struct roofline_sample_out * out, int (* bench_stat)(struct roofline_sample_out * , size_t));
unsigned roofline_PGCD(unsigned, unsigned);
unsigned roofline_PPCM(unsigned, unsigned);

long roofline_autoset_loop_repeat(void (* bench_fun)(struct roofline_sample_in *, struct roofline_sample_out *), struct roofline_sample_in * in, long ms_dur, unsigned long min_rep);

/******************************************* Hardware locality ***********************************/
extern hwloc_topology_t topology; /* The current machine topology */
extern size_t alignement;         /* The level 1 cache line size */
extern size_t LLC_size;           /* The last level cache size */
extern float  cpu_freq;           /* The cpu frequency defined by BENCHMARK_CPU_FREQ environment or at build time in the same #define directive */

int         roofline_hwloc_objtype_is_cache(hwloc_obj_type_t type);
int         roofline_hwloc_obj_snprintf(hwloc_obj_t obj, char * info_in, size_t n);
hwloc_obj_t roofline_hwloc_parse_obj(char*);
int         roofline_hwloc_cpubind();
int         roofline_hwloc_membind(hwloc_obj_t);
int         roofline_hwloc_obj_is_memory(hwloc_obj_t);
size_t      roofline_hwloc_get_memory_size(hwloc_obj_t);
hwloc_obj_t roofline_hwloc_get_next_memory(hwloc_obj_t);
hwloc_obj_t roofline_hwloc_get_previous_memory(hwloc_obj_t);
hwloc_obj_t roofline_hwloc_get_instruction_cache(void);
size_t      roofline_hwloc_get_instruction_cache_size(void);

/********************************************* Utils ********************************************/
#define errEXIT(msg) do{fprintf(stderr,msg"\n"); exit(EXIT_FAILURE);} while(0);
#define perrEXIT(msg) do{perror(msg); exit(EXIT_FAILURE);} while(0);
#define roofline_macro_str(x) #x
#define roofline_macro_xstr(x) roofline_macro_str(x)
#define roofline_ABS(x) ((x)>0 ? (x):-(x))
#define roofline_MAX(x, y) ((x)>(y) ? (x):(y))
#define roofline_MIN(x, y) ((x)<(y) ? (x):(y))
#define roofline_BOUND(a, x, y) (a>x ? (a<y?a:y) : x)
#define roofline_alloc(ptr,size) do{if(!(ptr=malloc(size))){perrEXIT("malloc");}} while(0)
#define roofline_realloc(ptr,size,max_size)				\
    do{									\
	while(size>=max_size){max_size*=2;}				\
	if((ptr = realloc(ptr,sizeof(*ptr)*max_size)) == NULL) perrEXIT("realloc"); \
    } while(0)

const char * roofline_type_str(int type);
void   roofline_print_header(FILE * output, const char * append);
void   roofline_print_sample(FILE * output, hwloc_obj_t obj, struct roofline_sample_out * sample_out, double sd, const char * append);

/**
 * Compute a logarithmic array of sizes
 * @arg start: The first element of the array. 
 * @arg end: The last element of the array. end
 * @arg n(in/out): the number of element in array.
 * @return A logarithmic array of sizes starting with start. Sizes are truncated to the closest integer value.
 **/
size_t * roofline_log_array(size_t start, size_t end, int * n);


/***************************************     PAPI sampling   *************************************/
#include <papi.h>
#define roofline_rdtsc(c_high,c_low) __asm__ __volatile__ ("CPUID\n\tRDTSC\n\tmovq %%rdx, %0\n\tmovq %%rax, %1\n\t" :"=r" (c_high), "=r" (c_low)::"%rax", "%rbx", "%rcx", "%rdx")
#define roofline_rdtsc_diff(high, low) ((high << 32) | low)
#if defined (__AVX512__)
#define SIMD_BYTES         64
#elif defined (__AVX__)
#define SIMD_BYTES         32
#elif defined (__SSE__)
#define SIMD_BYTES         16
#endif

extern struct roofline_sample_out output_sample;
extern FILE *                     output_file;

void roofline_sampling_init(const char * output);
void roofline_sampling_fini();
char * roofline_cat_info(const char *);
void roofline_eventset_init(int *);

#define roofline_sampling_start(info) do{				\
    uint64_t c_high_s, c_high_e, c_low_s, c_low_e;			\
    int       eventset = PAPI_NULL;					\
    long long values[5];						\
    uint64_t cycles = 0;						\
    hwloc_obj_t machine = hwloc_get_root_obj(topology);			\
    char * final_info = roofline_cat_info(info);			\
    _Pragma("omp single")						\
    _Pragma("{")							\
    output_sample.ts_start = 0;						\
    output_sample.ts_end = 0;						\
    output_sample.instructions = 0;					\
    output_sample.bytes = 0;						\
    output_sample.flops = 0;						\
    _Pragma("}")							\
    _Pragma("omp critical")						\
    roofline_eventset_init(&eventset);					\
    _Pragma("omp barrier")						\
    PAPI_start(eventset);						\
    roofline_rdtsc(c_high_s, c_low_s)

#define roofline_sampling_stop()					\
    PAPI_stop(eventset,values);						\
    roofline_rdtsc(c_high_e, c_low_e);					\
    _Pragma("omp barrier")						\
    cycles = roofline_rdtsc_diff(c_high_e, c_low_e) - roofline_rdtsc_diff(c_high_s, c_low_s); \
    _Pragma("omp critical")						\
    _Pragma("{")							\
    output_sample.ts_end = roofline_MAX(output_sample.ts_end, cycles);	\
    output_sample.instructions += values[0];				\
    output_sample.bytes += values[1]*SIMD_BYTES;			\
    output_sample.flops += values[2]+values[3]*2+values[4]*4;		\
    _Pragma("}")							\
    _Pragma("omp barrier")						\
    n_threads = omp_get_num_threads();					\
    PAPI_destroy_eventset(&eventset);					\
    _Pragma("omp single")						\
    roofline_print_sample(output_file, machine, &output_sample, 0, final_info);	\
    } while(0)

#endif /* BENCHMARK_H */
