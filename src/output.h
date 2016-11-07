#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdio.h>
#include <inttypes.h>
#include <hwloc.h>

typedef struct roofline_output_s{
    /* All sample type specific data */
    uint64_t ts_start;       /* Timestamp in cycles where the roofline started */
    uint64_t ts_end;         /* Timestamp in cycles where the roofline ended */
    uint64_t instructions;   /* The number of instructions */
    uint64_t bytes;          /* The amount of bytes transfered */
    uint64_t flops;          /* The amount of flops computed */
} roofline_output;

void roofline_output_clear(roofline_output * out);
void roofline_output_accumulate(roofline_output * dst, roofline_output * src);
int  roofline_compare_throughput(void * x, void * y);
void roofline_output_print(FILE * output,
			   const hwloc_obj_t src,
			   const hwloc_obj_t mem,
			   const roofline_output * sample_out,
			   const int type);
void roofline_output_print_header(FILE * output);

#endif /* OUTPUT_H */

