#ifndef BENCHMARK_H
#define BENCHMARK_H
#include <stdio.h>
#include <stdlib.h>
#include <hwloc.h>
#include "types.h"

/***********************************  LIB GLOBALS ****************************************/

extern hwloc_topology_t topology;   /* Current machine topology */
extern float            cpu_freq;   /* In Hz */
extern unsigned         n_threads;  /* Set number of threads */
extern size_t           max_size;   /* The 32 * last level cache size */
extern hwloc_obj_t      root;       /* The subroot of topology to select the amount of threads */

int  roofline_lib_init(hwloc_topology_t topology, int with_hyperthreading, int whole_system);
void roofline_lib_finalize(void);

void roofline_flops    (FILE * output, const int op_type);
void roofline_bandwidth(FILE * output, const hwloc_obj_t memory, const int op_type);
void roofline_oi       (FILE * output, const hwloc_obj_t memory, const int op_type, const unsigned flops, const unsigned bytes);


/****************************************    Statistics    ****************************************/


/**
 * Compute a logarithmic array of sizes
 * @arg start: The first element of the array. 
 * @arg end: The last element of the array. end
 * @arg n(in/out): the number of element in array.
 * @return A logarithmic array of sizes starting with start. Sizes are truncated to the closest integer value.
 **/
size_t * roofline_log_array(size_t start, size_t end, int * n);

#endif /* BENCHMARK_H */
