#include "intel.h"

/* asm instruction for flops between operand a & b. Result stored into c*/
#define simd_fp(op1, op2)			\
  roofline_flop_ins(op1,"0")			\
  roofline_flop_ins(op2,"1")			\
  roofline_flop_ins(op1,"2")			\
  roofline_flop_ins(op2,"3")			\
  roofline_flop_ins(op1,"4")			\
  roofline_flop_ins(op2,"5")			\
  roofline_flop_ins(op1,"6")			\
  roofline_flop_ins(op2,"7")			\
  roofline_flop_ins(op1,"8")			\
  roofline_flop_ins(op2,"9")			\
  roofline_flop_ins(op1,"10")			\
  roofline_flop_ins(op2,"11")			\
  roofline_flop_ins(op1,"12")			\
  roofline_flop_ins(op2,"13")			\
  roofline_flop_ins(op1,"14")			\
  roofline_flop_ins(op2,"15")

#define asm_flops(repeat, out, type_str, op1, op2) do{	\
    roofline_rdtsc(c_high, c_low);			\
    __asm__ __volatile__ (				\
      "loop_flops_"type_str"_repeat:\n\t"		\
      simd_fp(op1,op2)					\
      "sub $1, %0\n\t"					\
      "jnz loop_flops_"type_str"_repeat\n\t"		\
      :: "r" (repeat)	: SIMD_CLOBBERED_REGS);		\
    roofline_rdtsc(c_high1, c_low1);			\
  } while(0)


void benchmark_flops(long repeat, roofline_output * out, int op_type){
  volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
  out->instructions = 16*repeat;
  out->flops = out->instructions * SIMD_FLOPS;

  zero_simd();
  switch(op_type){
  case ROOFLINE_ADD:
    asm_flops(repeat, out, "add", SIMD_ADD, SIMD_ADD);
    break;
  case ROOFLINE_MUL:
    asm_flops(repeat, out, "mul", SIMD_MUL, SIMD_MUL);
    break;
  case ROOFLINE_MAD:
    asm_flops(repeat, out, "mad", SIMD_MUL, SIMD_ADD);
    break;
#if defined (__FMA__)
  case ROOFLINE_FMA:
    asm_flops(repeat, out, "fma", SIMD_FMA, SIMD_FMA);
    out->flops*=2;
    break;
#endif
  default:
    ERR("Not supported type %s\n", roofline_type_str(op_type));
    break;
  }

  out->ts_start = roofline_rdtsc_diff(c_high, c_low);
  out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
}

