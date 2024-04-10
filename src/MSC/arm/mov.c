#include "arm.h"
#include "../../list.h"

#define asm_mov(op1, op2, reg)			\
  op1(STRIDE_0,reg,REG_0)			\
  op1(STRIDE_1,reg,REG_1)			\
  op2(STRIDE_2,reg,REG_2)			\
  op1(STRIDE_3,reg,REG_3)			\
  op1(STRIDE_4,reg,REG_4)			\
  op2(STRIDE_5,reg,REG_5)			\
  op1(STRIDE_6,reg,REG_6)			\
  op1(STRIDE_7,reg,REG_7)			\
  op2(STRIDE_8,reg,REG_8)			\
  op1(STRIDE_9,reg,REG_9)			\
  op1(STRIDE_10,reg,REG_10)			\
  op2(STRIDE_11,reg,REG_11)			\
  op1(STRIDE_12,reg,REG_12)			\
  op1(STRIDE_13,reg,REG_13)			\
  op2(STRIDE_14,reg,REG_14)			\
  op1(STRIDE_15,reg,REG_15)			
//  op1(STRIDE_16,reg,REG_16)			\
//  op2(STRIDE_17,reg,REG_17)			
//   op1(STRIDE_18,reg,REG_18)			\
//   op1(STRIDE_19,reg,REG_19)			\
//   op2(STRIDE_20,reg,REG_20)			\
//   op1(STRIDE_21,reg,REG_21)			\
//   op1(STRIDE_22,reg,REG_22)			\
//   op2(STRIDE_23,reg,REG_23)			\
//   op1(STRIDE_24,reg,REG_24)			\
//   op1(STRIDE_25,reg,REG_25)			\
//   op2(STRIDE_26,reg,REG_26)			\
//   op1(STRIDE_27,reg,REG_27)			\
//   op1(STRIDE_28,reg,REG_28)			\
//   op2(STRIDE_29,reg,REG_29)

#define asm_benchmark_mov(repeat, data, out, op1, op2, op_str) do{  \
  zero_simd();                                \
  roofline_output_begin_measure(out);                  \
   __asm__ __volatile__ (                        \
    "loop_"op_str"_repeat:\n\t"                    \
    "ldr x11, %1\n\t"                          \
    "ldr x12, %2\n\t"                          \
    "buffer_"op_str"_increment:\n\t"               \
    asm_mov(op1, op2, "x11")                       \
    "add x11, x11, #"STRINGIFY(CHUNK_SIZE)"\n\t"       \
    "sub x12, x12, #"STRINGIFY(CHUNK_SIZE)"\n\t"       \
    "cmp x12, #0\n\t"                          \
    "bne buffer_"op_str"_increment\n\t"             \
    "subs %0, %0, #1\n\t"                       \
    "bne loop_"op_str"_repeat\n\t"                  \
    :: "r" (repeat), "r" (data->stream), "r" (data->size)    \
    : "x11", "x12", "memory");  \
  roofline_output_end_measure(out, (data->size)*repeat, 0, repeat*data->size/SIMD_BYTES); \
  } while(0)
#define asm_mov_overhead(data, out) do { \
    zero_simd(); \
    roofline_output_begin_measure(out); \
    unsigned long n = 1; /* Use a variable as an lvalue instead of a literal */ \
    __asm__ __volatile__ ( \
        "mov x11, %[stream]\n\t" \
        "mov x12, %[size]\n\t" \
        "1:\n\t" \
        "add x11, x11, #" STRINGIFY(CHUNK_SIZE) "\n\t" \
        "sub x12, x12, #" STRINGIFY(CHUNK_SIZE) "\n\t" \
        "cmp x12, #0\n\t" \
        "bne 1b\n\t" \
        "subs %[n], %[n], #1\n\t" \
        "bne 1b\n\t" \
        : [n] "+r" (n) \
        : [stream] "r" (data->stream), [size] "r" (data->size) \
        : "x11", "x12", "memory", "cc" \
    ); \
    roofline_output_end_measure(out, 0, 0, 0); \
} while(0)


void benchmark_mov(roofline_stream data, roofline_output out, long repeat, int op_type){
  /* Overhead measure */
  asm_mov_overhead(data, out);
  
  switch(op_type){
  case ROOFLINE_LOAD:
    asm_benchmark_mov(repeat, data, out, roofline_load_ins, roofline_load_ins, "load");
    break;
//  case ROOFLINE_LOAD_NT:
//    asm_benchmark_mov(repeat, data, out, roofline_loadnt_ins, roofline_loadnt_ins, "loadnt");
//    break;
  case ROOFLINE_STORE:
    asm_benchmark_mov(repeat, data, out, roofline_store_ins, roofline_store_ins, "store");
    break;
//  case ROOFLINE_STORE_NT:
//    asm_benchmark_mov(repeat, data, out, roofline_storent_ins, roofline_storent_ins, "storent");
//    break;
  case ROOFLINE_2LD1ST:
    asm_benchmark_mov(repeat, data, out, roofline_load_ins, roofline_store_ins, "2ld1st");
    break;
  default:
    ERR("Call to %s with not supported type %s\n", __FUNCTION__, roofline_type_str(op_type));
    break;
  }
}

