#include "output.h"
#include "types.h"
#include "utils.h"
#include "topology.h"

extern float    cpu_freq;       /* In Hz */
extern unsigned n_threads;      /* The number of threads for benchmark */

void roofline_output_clear(roofline_output * out){
  out->ts_start = 0;
  out->ts_end = 0;
  out->bytes = 0;
  out->flops = 0;
  out->instructions = 0;
}

void roofline_output_accumulate(roofline_output * dst, roofline_output * src){
  dst->ts_start = src->ts_start;
  dst->ts_end = src->ts_end;
  dst->bytes += src->bytes;
  dst->flops += src->flops;
  dst->instructions += src->instructions;
}

int roofline_compare_throughput(void * x, void * y){
  roofline_output * a = *(roofline_output **)x;
  roofline_output * b = *(roofline_output **)y;
  float a_throughput = a->instructions / (float)(a->ts_end - a->ts_start);
  float b_throughput = b->instructions / (float)(b->ts_end - b->ts_start);
  if(a_throughput < b_throughput) return -1;
  if(a_throughput > b_throughput) return  1;
  return 0;
}

void roofline_output_print_header(FILE * output){
  fprintf(output, "%12s %10s %12s %10s %10s %10s %10s %10s\n",
	  "location", "n_threads", "memory", "throughput", "GByte/s", "GFlop/s", "Flops/Byte", "type");
}

void roofline_output_print(FILE * output,
			   const hwloc_obj_t src,
			   const hwloc_obj_t mem,
			   const roofline_output * sample_out,
			   const int type)
{
  roofline_mkstr(src_str,12);
  roofline_mkstr(mem_str,12);
  roofline_hwloc_obj_snprintf(src, src_str, sizeof(src_str));
  if(mem != NULL) roofline_hwloc_obj_snprintf(mem, mem_str, sizeof(mem_str));
  else snprintf(mem_str, sizeof(mem_str), "NA");

  long cyc = sample_out->ts_end - sample_out->ts_start;

  fprintf(output, "%12s %10u %12s %10.3f %10.3f %10.3f %10.6f %10s\n",
	  src_str,
	  n_threads,
	  mem_str, 
	  (float)sample_out->instructions / (float) cyc,
	  (float)(sample_out->bytes * cpu_freq) / (float)(cyc*1e9), 
	  (float)(sample_out->flops * cpu_freq) / (float)(1e9*cyc),
	  (float)(sample_out->flops) / (float)(sample_out->bytes),
	  roofline_type_str(type));
  fflush(output);
}

