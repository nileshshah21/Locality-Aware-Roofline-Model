#include "../roofline.h"

/**
 * Compute and allocate aligned data of size size greater than the provided size which fits a chunk size.
 * @param data: A pointer to the data to be allocated. If NULL nothing is allocated.
 * @param size: A reference size to allocate.
 * @return The size of allocated chunk.
 *
 **/
size_t alloc_chunk_aligned(void ** data, size_t size);
void fpeak_bench(struct roofline_sample_in * in, struct roofline_sample_out * out);

/**
 * Create, compile and load the roofline function according to a specified operational intensity.
 * @param oi: The operational intensity to perform.
 * @param type: A type of memory operation: load or store.
 * @return The benchmark generating given operational intensity.
 **/
void (*roofline_oi_bench(double oi, int type)) (struct roofline_sample_in * in, struct roofline_sample_out * out);

#define roofline_rdtsc(c_high,c_low) __asm__ __volatile__ ("CPUID\n\tRDTSC\n\tmovq %%rdx, %0\n\tmovq %%rax, %1\n\t" :"=r" (c_high), "=r" (c_low)::"%rax", "%rbx", "%rcx", "%rdx")
#define roofline_rdtsc_diff(high, low) ((high << 32) | low)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


