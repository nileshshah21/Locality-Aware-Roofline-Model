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
  out->n = 0;
  out->overhead = 0;
}

void roofline_output_accumulate(roofline_output dst, const roofline_output src){    
  dst->cycles += src->cycles;
  dst->bytes += src->bytes;
  dst->flops += src->flops;
  dst->instructions += src->instructions;
  dst->overhead += src->overhead;
  dst->n++;
  
  /* merge cpusets */  
  roofline_hwloc_accumulate(&(dst->thr_location), &(src->thr_location));
  roofline_hwloc_accumulate(&(dst->mem_location), &(src->mem_location));  
}

roofline_output new_roofline_output(hwloc_obj_t mem_location){
  hwloc_obj_t location = roofline_hwloc_get_cpubind();  
  roofline_output o = malloc(sizeof(*o));
  if(o == NULL) return NULL;
  roofline_output_clear(o);
  o->mem_location = mem_location;
  o->thr_location = location;
  return o;
}

void delete_roofline_output(roofline_output o){
  free(o);
}

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
  __asm__ __volatile__ ("CPUID\n\t" "RDTSC\n\t" "movq %%rdx, %0\n\t" "movq %%rax, %1\n\t" \
			:"=r" (o->ts_high_end), "=r" (o->ts_low_end)::"%rax", "%rbx", "%rcx", "%rdx");
#ifdef _OPENMP
#pragma omp barrier
#endif  
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
			   const roofline_output out,
			   const int type)
{
  roofline_mkstr(src_str,12);
  roofline_mkstr(mem_str,12);
  if(out->thr_location != NULL) roofline_hwloc_obj_snprintf(out->thr_location, src_str, sizeof(src_str));
  else snprintf(src_str, sizeof(src_str), "NA");
  if(out->mem_location != NULL) roofline_hwloc_obj_snprintf(out->mem_location, mem_str, sizeof(mem_str));
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


roofline_output roofline_output_init(){
  unsigned i, n = roofline_hwloc_n_memory_group();
  roofline_output out;
  roofline_alloc(out, sizeof(struct roofline_output_s)*n);
  for(i=0;i<n; i++){
    roofline_output_clear(out+i);
    out[i].mem_location = NULL;
    out[i].thr_location = roofline_hwloc_memory_group(i);      
  }
  return out;
}

void roofline_output_aggregate_result(roofline_output results, const roofline_output src){
  roofline_output ret  = results;
  hwloc_obj_t location = src->thr_location;
  int groupd = roofline_hwloc_memory_group_depth();

  if(location){
    if(location->depth < groupd){
      location = roofline_hwloc_memory_group(0);
    }
    else if(location && location->type == HWLOC_OBJ_NUMANODE){
      location = location->parent;
    }
    else {
      while(location && location->depth > groupd){ location = location->parent; }
    }
  } else {    
    location = roofline_hwloc_memory_group(0);
  }

  ret = &(results[location->logical_index]);
  roofline_output_accumulate(ret, src);
}

void roofline_output_fini(const roofline_output output){
  free(output);
}

void roofline_print_outputs(FILE * output, const roofline_output outputs, const int op_type){
  int j;
  for(j=0; j<hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_GROUP); j++){
    if(outputs[j].n > 0){
      roofline_output_print(output, outputs+j, op_type);
      roofline_output_clear(outputs+j);
    }
  }
}

