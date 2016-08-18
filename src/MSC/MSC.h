#include "../roofline.h"

extern size_t chunk_size;

int  benchmark_types_supported();
void fpeak_benchmark(    const struct roofline_sample_in * in, struct roofline_sample_out * out, int type);
void bandwidth_benchmark(const struct roofline_sample_in * in, struct roofline_sample_out * out, int type);
/**
 * Create, compile and load the roofline function according to a specified operational intensity.
 * @param oi: The operational intensity to perform.
 * @param type: A type of memory operation: load or store.
 * @return The benchmark generating given operational intensity.
 **/
void (*roofline_oi_bench(const double oi, const int type)) (const struct roofline_sample_in * in, struct roofline_sample_out * out, int type);

#define roofline_rdtsc(c_high,c_low) __asm__ __volatile__ ("CPUID\n\tRDTSC\n\tmovq %%rdx, %0\n\tmovq %%rax, %1\n\t" :"=r" (c_high), "=r" (c_low)::"%rax", "%rbx", "%rcx", "%rdx")
#define roofline_rdtsc_diff(high, low) ((high << 32) | low)


