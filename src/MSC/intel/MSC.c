#include "intel.h"

void
benchmark_fpeak(int op_type, roofline_output * out, long repeat)
{
  benchmark_flops(repeat, out, op_type);
}

void
benchmark_single_stream(roofline_stream data, roofline_output * out, int op_type, long repeat)
{
  benchmark_mov(data, out, repeat, op_type);
}

void
benchmark_double_stream(roofline_stream dst, roofline_stream src, roofline_output * out, int op_type, long repeat)
{
  switch(op_type){
  case ROOFLINE_COPY:
    benchmark_copy(dst, src, out, repeat);
    break;
  default:
    ERR("Call to %s with not supported type %s\n", __FUNCTION__, roofline_type_str(op_type));
    break;
  }
}

#if defined (__AVX512__)
#elif defined (__AVX__)
#elif (__SSE__) || defined (__SSE2__) || defined (__SSE4_1__)
#endif

size_t oi_chunk_size;

size_t get_chunk_size(int op_type){
  if(op_type & ~(ROOFLINE_LOAD|ROOFLINE_LOAD_NT|ROOFLINE_STORE|ROOFLINE_STORE_NT|ROOFLINE_2LD1ST|ROOFLINE_COPY))
    return oi_chunk_size;
  else if(op_type & (ROOFLINE_LOAD|ROOFLINE_LOAD_NT|ROOFLINE_STORE|ROOFLINE_STORE_NT|ROOFLINE_2LD1ST|ROOFLINE_COPY))
    return CHUNK_SIZE;
  else return 0;
}

int benchmark_types_supported(){
  int supported = ROOFLINE_LOAD|ROOFLINE_STORE|ROOFLINE_2LD1ST|ROOFLINE_COPY|ROOFLINE_MUL|ROOFLINE_ADD|ROOFLINE_MAD;
#ifdef __FMA__
  supported = supported | ROOFLINE_FMA;
#endif
#if defined (__AVX512__ ) || defined (__AVX2__)
  supported = supported | ROOFLINE_LOAD_NT;
#endif
#if defined (__AVX512__ ) || defined (__AVX2__)  || defined (__AVX2__) || defined (_AVX_) || defined (_SSE_4_1_) || defined (_SSE2_)
  supported = supported | ROOFLINE_STORE_NT;
#endif
  return supported;
}

void validation_single_stream(roofline_stream data,
			      roofline_output * out,
			      int op_type,
			      long repeat,
			      unsigned flops,
			      unsigned bytes);

void validation_double_stream(roofline_stream dst,
			      roofline_stream src,
			      roofline_output * out,
			      int op_type,
			      long repeat,
			      unsigned flops,
			      unsigned bytes);

