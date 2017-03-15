#include "output.h"
#include "types.h"
#include "utils.h"
#include "topology.h"

extern float    cpu_freq;       /* In Hz */

void roofline_output_clear(roofline_output out){
  out->ts_high_start = 0;
  out->ts_high_end = 0;
  out->ts_low_start = 0;
  out->ts_low_end = 0;
  out->cycles = 0;  
  out->bytes = 0;
  out->flops = 0;
  out->instructions = 0;
  out->n = 1;
  out->overhead = 0;
}

void roofline_output_accumulate(roofline_output dst, const roofline_output src){
  dst->cycles += src->cycles;
  dst->bytes += src->bytes;
  dst->flops += src->flops;
  dst->instructions += src->instructions;
  dst->overhead += src->overhead;
  dst->n++;
}

roofline_output new_roofline_output(){
  roofline_output o = malloc(sizeof(*o));
  if(o == NULL) return NULL;
  roofline_output_clear(o);
  return o;
}

roofline_output roofline_output_copy(roofline_output o){
  roofline_output out = malloc(sizeof(*out));
  if(out == NULL) return NULL;
  out->ts_high_start = o->ts_high_start;
  out->ts_high_end = o->ts_high_end;
  out->ts_low_start = o->ts_low_start;
  out->ts_low_end = o->ts_low_end;
  out->cycles = 0;o->cycles;
  out->bytes = o->bytes;
  out->flops = o->flops;
  out->instructions = o->instructions;
  out->n = o->n;
  out->overhead = o->overhead;
  return out;
}

void delete_roofline_output(roofline_output o){ free(o); }

float roofline_output_throughput(const roofline_output s){
  return (float)(s->instructions)/(float)(s->cycles);
}

void roofline_output_begin_measure(roofline_output o){
#ifdef _OPENMP
#pragma omp barrier
#endif
  __asm__ __volatile__ ("CPUID\n\t" "RDTSC\n\t" "movq %%rdx, %0\n\t" "movq %%rax, %1\n\t" \
			:"=r" (o->ts_high_start), "=r" (o->ts_low_start)::"%rax", "%rbx", "%rcx", "%rdx");
}

void roofline_output_end_measure(roofline_output o, const uint64_t bytes, const uint64_t flops, const uint64_t ins){
#ifdef _OPENMP
#pragma omp barrier
#endif
  __asm__ __volatile__ ("CPUID\n\t" "RDTSC\n\t" "movq %%rdx, %0\n\t" "movq %%rax, %1\n\t" \
			:"=r" (o->ts_high_end), "=r" (o->ts_low_end)::"%rax", "%rbx", "%rcx", "%rdx");
  if((flops == 0 && bytes == 0) || ins == 0){
    o->overhead = (o->ts_high_end<<32|o->ts_low_end)-(o->ts_high_start<<32|o->ts_low_start);
  } else {
    o->cycles = (o->ts_high_end<<32|o->ts_low_end)-(o->ts_high_start<<32|o->ts_low_start);
    o->flops = flops;
    o->bytes = bytes;
    o->instructions = ins;    
  }
}

int roofline_compare_throughput(const void * x, const void * y){
  float x_thr = roofline_output_throughput(*(roofline_output*)x);
  float y_thr = roofline_output_throughput(*(roofline_output*)y);
  if(x_thr < y_thr) return -1;
  if(x_thr > y_thr) return  1;
  return 0;
}

int roofline_compare_cycles(const void * x, const void * y){
  roofline_output a = *(roofline_output*)x;
  roofline_output b = *(roofline_output*)y;
  if(a->cycles < b->cycles) return -1;
  if(a->cycles > b->cycles) return  1;
  return 0;
}


void roofline_output_print_header(FILE * output){
  fprintf(output, "%12s %10s %12s %10s %10s %10s %10s %10s\n",
	  "location", "n_threads", "memory", "throughput", "GByte/s", "GFlop/s", "Flops/Byte", "type");
}

void roofline_output_print(FILE * output,
			   const hwloc_obj_t src,
			   const hwloc_obj_t mem,
			   const roofline_output out,
			   const int type)
{
  roofline_mkstr(src_str,12);
  roofline_mkstr(mem_str,12);
  roofline_hwloc_obj_snprintf(src, src_str, sizeof(src_str));
  if(mem != NULL) roofline_hwloc_obj_snprintf(mem, mem_str, sizeof(mem_str));
  else snprintf(mem_str, sizeof(mem_str), "NA");

  float    cycles               = (float)(out->cycles-out->overhead)/(float)(out->n);
  float    throughput           = (float)(out->instructions)/cycles;
  float    bandwidth            = (float)(out->bytes*cpu_freq)/(cycles*1e9);
  float    performance          = (float)(out->flops*cpu_freq)/(cycles*1e9);
  float    arithmetic_intensity = (float)(out->flops)/(float)(out->bytes);      

  /* printf("overhead = %lu cyc, cycles = %lu\n", out->overhead, out->cycles); */
  fprintf(output, "%12s %10u %12s %10.3f %10.3f %10.3f %10.6f %10s\n",
	  src_str, out->n, mem_str, throughput, bandwidth, performance, arithmetic_intensity, roofline_type_str(type));
  
  fflush(output);
}

