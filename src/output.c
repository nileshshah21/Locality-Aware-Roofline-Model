#include "output.h"
#include "types.h"

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
  fprintf(output, "%12s %10s %10s %10s %10s %10s %10s\n",
	  "Obj", "Throughput", "GByte/s", "GFlop/s", "Flops/Byte", "n_threads", "type");
}

void roofline_output_print(FILE * output, const hwloc_obj_t obj, const roofline_output * sample_out, const int type)
{
  char obj_str[12];
  long cyc = sample_out->ts_end - sample_out->ts_start;
  memset(obj_str,0,sizeof(obj_str));
  hwloc_obj_type_snprintf(obj_str, 10, obj, 0);
  snprintf(obj_str+strlen(obj_str),5,":%d",obj->logical_index);
  fprintf(output, "%12s %10.2f %10.2f %10.2f %10.6f %10u %10s\n",
	  obj_str, 
	  (float)sample_out->instructions / (float) cyc,
	  (float)(sample_out->bytes * cpu_freq) / (float)(cyc*1e9), 
	  (float)(sample_out->flops * cpu_freq) / (float)(1e9*cyc),
	  (float)(sample_out->flops) / (float)(sample_out->bytes),
	  n_threads,
	  roofline_type_str(type));
  fflush(output);
}

