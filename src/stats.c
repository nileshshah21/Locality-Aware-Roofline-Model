#include "MSC/MSC.h"
#include "types.h"
#include "utils.h"
#include "output.h"

extern float cpu_freq;   /* In Hz */

unsigned roofline_PGCD(unsigned a, unsigned b){
  unsigned max,min, r;
  max = roofline_MAX(a,b);
  min = roofline_MIN(a,b);
  while((r=max%min)!=0){max = min; min = r;}
  return min;
}

unsigned roofline_PPCM(unsigned a, unsigned b){
  return a*b/roofline_PGCD(a,b);
}

long roofline_autoset_repeat(roofline_stream dst, roofline_stream src, const int op_type, const void * benchmark)
{
  long repeat = 1;
  float mul = 0, tv_ms = -1;
  roofline_output out;
  void (*  benchmark_function)(roofline_stream, roofline_output *, int, long) = benchmark;
  while(tv_ms < ROOFLINE_MIN_DUR){
    roofline_output_clear(&out);
    if(op_type == ROOFLINE_ADD ||
       op_type == ROOFLINE_MUL ||
       op_type == ROOFLINE_MAD ||
       op_type == ROOFLINE_FMA){
      benchmark_fpeak(op_type, &out, repeat);
    }
    else if(op_type == ROOFLINE_LOAD    ||
	    op_type == ROOFLINE_LOAD_NT ||
	    op_type == ROOFLINE_STORE   ||
	    op_type == ROOFLINE_STORE_NT||
	    op_type == ROOFLINE_2LD1ST){
      benchmark_single_stream(src, &out, op_type, repeat);
    }
    else if(op_type  == ROOFLINE_COPY){
      benchmark_double_stream(dst, src, &out, op_type, repeat);
    }
    else if(benchmark != NULL){
      benchmark_function(src, &out, op_type, repeat);
    }
    else{
      ERR("Not suitable op_type %s\n", roofline_type_str(op_type));
      return 1;
    }
    tv_ms = (out.ts_end - out.ts_start)*1e3/cpu_freq;
    if( tv_ms < ROOFLINE_MIN_DUR){
      mul = (float)(ROOFLINE_MIN_DUR)/tv_ms;
      if((long)mul <= 1)
        repeat *= 2;
      else
	repeat *= mul;
    }
  }

  roofline_debug1("repeat loop %ld times\n", repeat);
  return repeat;
}


