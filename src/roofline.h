#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdio.h>
#include <stdlib.h>
#include <hwloc.h>

extern hwloc_topology_t topology;   /* Current machine topology */
extern float        cpu_freq;       /* In Hz */
extern unsigned     n_threads;      /* The number of threads for benchmark */
extern char *       compiler;       /* The compiler name to compile the roofline validation. */
extern char *       omp_flag;       /* The openmp flag to compile the roofline validation. */
extern int          per_thread;     /* Should results be printed with per thread value */

int  roofline_lib_init(hwloc_topology_t topology, int with_hyperthreading, int whole_system);
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
#define ROOFLINE_LOAD     1   /* benchmark type */
#define ROOFLINE_LOAD_NT  2   /* benchmark type */
#define ROOFLINE_STORE    4   /* benchmark type */
#define ROOFLINE_STORE_NT 8   /* benchmark type */
#define ROOFLINE_2LD1ST   16  /* benchmark type */
#define ROOFLINE_COPY     32  /* benchmark type */
#define ROOFLINE_MUL      64  /* benchmark type */
#define ROOFLINE_ADD      128 /* benchmark type */
#define ROOFLINE_MAD      256 /* benchmark type */
#define ROOFLINE_FMA      512 /* benchmark type */

#ifndef ROOFLINE_N_SAMPLES
#define ROOFLINE_N_SAMPLES 8
#endif

int  roofline_filter_types(hwloc_obj_t obj, int type);
void roofline_flops    (FILE * output, const int type);
void roofline_bandwidth(FILE * output, hwloc_obj_t memory, const int type);
void roofline_oi       (FILE * output, hwloc_obj_t memory, const int type, double oi);

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
#define BENCHMARK_REPEAT 8 /* Repeat fpeak benchmark */
#ifndef BENCHMARK_MIN_DUR
#define BENCHMARK_MIN_DUR 10 /* milliseconds */
#endif

unsigned roofline_PGCD(unsigned, unsigned);
unsigned roofline_PPCM(unsigned, unsigned);

long roofline_autoset_loop_repeat(void (* bench_fun)(const struct roofline_sample_in *, struct roofline_sample_out *, int), struct roofline_sample_in * in, const int type, long ms_dur, unsigned long min_rep);

/******************************************* Hardware locality ***********************************/
extern hwloc_topology_t topology; /* The current machine topology */
extern size_t alignement;         /* The level 1 cache line size */
extern size_t LLC_size;           /* The last level cache size */
extern float  cpu_freq;           /* The cpu frequency defined by BENCHMARK_CPU_FREQ environment or at build time in the same #define directive */

int         roofline_hwloc_objtype_is_cache(hwloc_obj_type_t type);
int         roofline_hwloc_obj_snprintf(hwloc_obj_t obj, char * info_in, size_t n);
hwloc_obj_t roofline_hwloc_parse_obj(char*);
int         roofline_hwloc_cpubind(hwloc_obj_t);
int         roofline_hwloc_membind(hwloc_obj_t);
int         roofline_hwloc_obj_is_memory(hwloc_obj_t);
size_t      roofline_hwloc_get_memory_size(hwloc_obj_t);
hwloc_obj_t roofline_hwloc_get_next_memory(hwloc_obj_t, int vertical);
hwloc_obj_t roofline_hwloc_get_under_memory(hwloc_obj_t, int whole_system);
hwloc_obj_t roofline_hwloc_get_instruction_cache(void);
size_t      roofline_hwloc_get_instruction_cache_size(void);

/********************************************* Utils ********************************************/
#define errEXIT(msg) do{fprintf(stderr,msg"\n"); exit(EXIT_FAILURE);} while(0);
#define perrEXIT(msg) do{perror(msg); exit(EXIT_FAILURE);} while(0);
#define roofline_str(x) #x
#define roofline_stringify(x) roofline_str(x)
#define roofline_MAX(x, y) ((x)>(y) ? (x):(y))
#define roofline_MIN(x, y) ((x)<(y) ? (x):(y))
#define roofline_alloc(ptr,size) do{if(!(ptr=malloc(size))){perrEXIT("malloc");}} while(0)

const char * roofline_type_str(int type);
int          roofline_type_from_str(const char * type);
void         roofline_print_header(FILE * output);
void         roofline_print_sample(FILE * output, hwloc_obj_t obj, struct roofline_sample_out * sample_out, int type);

/**
 * Compute a logarithmic array of sizes
 * @arg start: The first element of the array. 
 * @arg end: The last element of the array. end
 * @arg n(in/out): the number of element in array.
 * @return A logarithmic array of sizes starting with start. Sizes are truncated to the closest integer value.
 **/
size_t * roofline_log_array(size_t start, size_t end, int * n);

#endif /* BENCHMARK_H */
