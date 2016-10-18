#include "intel.h"

#define asm_copy(src, dst)			\
  roofline_loadnt_ins(STRIDE_0,src,REG_0)	\
  roofline_storent_ins(STRIDE_0,dst,REG_0)	\
  roofline_loadnt_ins(STRIDE_1,src,REG_1)	\
  roofline_storent_ins(STRIDE_1,dst,REG_1)	\
  roofline_loadnt_ins(STRIDE_2,src,REG_2)	\
  roofline_storent_ins(STRIDE_2,dst,REG_2)	\
  roofline_loadnt_ins(STRIDE_3,src,REG_3)	\
  roofline_storent_ins(STRIDE_3,dst,REG_3)	\
  roofline_loadnt_ins(STRIDE_4,src,REG_4)	\
  roofline_storent_ins(STRIDE_4,dst,REG_4)	\
  roofline_loadnt_ins(STRIDE_5,src,REG_5)	\
  roofline_storent_ins(STRIDE_5,dst,REG_5)	\
  roofline_loadnt_ins(STRIDE_6,src,REG_6)	\
  roofline_storent_ins(STRIDE_6,dst,REG_6)	\
  roofline_loadnt_ins(STRIDE_7,src,REG_7)	\
  roofline_storent_ins(STRIDE_7,dst,REG_7)	\
  roofline_loadnt_ins(STRIDE_8,src,REG_8)	\
  roofline_storent_ins(STRIDE_8,dst,REG_8)	\
  roofline_loadnt_ins(STRIDE_9,src,REG_9)	\
  roofline_storent_ins(STRIDE_9,dst,REG_9)	\
  roofline_loadnt_ins(STRIDE_10,src,REG_10)	\
  roofline_storent_ins(STRIDE_10,dst,REG_10)	\
  roofline_loadnt_ins(STRIDE_11,src,REG_11)	\
  roofline_storent_ins(STRIDE_11,dst,REG_11)	\
  roofline_loadnt_ins(STRIDE_12,src,REG_12)	\
  roofline_storent_ins(STRIDE_12,dst,REG_12)	\
  roofline_loadnt_ins(STRIDE_13,src,REG_13)	\
  roofline_storent_ins(STRIDE_13,dst,REG_13)	\
  roofline_loadnt_ins(STRIDE_14,src,REG_14)	\
  roofline_storent_ins(STRIDE_14,dst,REG_14)	\
  roofline_loadnt_ins(STRIDE_15,src,REG_15)	\
  roofline_storent_ins(STRIDE_15,dst,REG_15)	\
  roofline_loadnt_ins(STRIDE_16,src,REG_16)	\
  roofline_storent_ins(STRIDE_16,dst,REG_16)	\
  roofline_loadnt_ins(STRIDE_17,src,REG_17)	\
  roofline_storent_ins(STRIDE_17,dst,REG_17)	\
  roofline_loadnt_ins(STRIDE_18,src,REG_18)	\
  roofline_storent_ins(STRIDE_18,dst,REG_18)	\
  roofline_loadnt_ins(STRIDE_19,src,REG_19)	\
  roofline_storent_ins(STRIDE_19,dst,REG_19)	\
  roofline_loadnt_ins(STRIDE_20,src,REG_20)	\
  roofline_storent_ins(STRIDE_20,dst,REG_20)	\
  roofline_loadnt_ins(STRIDE_21,src,REG_21)	\
  roofline_storent_ins(STRIDE_21,dst,REG_21)	\
  roofline_loadnt_ins(STRIDE_22,src,REG_22)	\
  roofline_storent_ins(STRIDE_22,dst,REG_22)	\
  roofline_loadnt_ins(STRIDE_23,src,REG_23)	\
  roofline_storent_ins(STRIDE_23,dst,REG_23)	\
  roofline_loadnt_ins(STRIDE_24,src,REG_24)	\
  roofline_storent_ins(STRIDE_24,dst,REG_24)	\
  roofline_loadnt_ins(STRIDE_25,src,REG_25)	\
  roofline_storent_ins(STRIDE_25,dst,REG_25)	\
  roofline_loadnt_ins(STRIDE_26,src,REG_26)	\
  roofline_storent_ins(STRIDE_26,dst,REG_26)	\
  roofline_loadnt_ins(STRIDE_27,src,REG_27)	\
  roofline_storent_ins(STRIDE_27,dst,REG_27)	\
  roofline_loadnt_ins(STRIDE_28,src,REG_28)	\
  roofline_storent_ins(STRIDE_28,dst,REG_28)	\
  roofline_loadnt_ins(STRIDE_29,src,REG_29)	\
  roofline_storent_ins(STRIDE_29,dst,REG_29)

void benchmark_copy(roofline_stream dst, roofline_stream src, roofline_output * out, long repeat){
  uint64_t c_low0=0, c_low1=0, c_high0=0, c_high1=0;
  zero_simd();
  roofline_rdtsc(c_high0, c_low0);
  __asm__ __volatile__ (						\
    "loop_copy_repeat:\n\t"						\
    "mov %1, %%r11\n\t"							\
    "mov %3, %%r12\n\t"							\
    "mov %2, %%r13\n\t"							\
    "buffer_copy_increment:\n\t"					\
    asm_copy("%%r11", "%%r12")						\
    "add $"STRINGIFY(CHUNK_SIZE)", %%r11\n\t"				\
    "add $"STRINGIFY(CHUNK_SIZE)", %%r12\n\t"				\
    "sub $"STRINGIFY(CHUNK_SIZE)", %%r13\n\t"				\
    "jnz buffer_copy_increment\n\t"					\
    "sub $1, %0\n\t"							\
    "jnz loop_copy_repeat\n\t"						\
    :: "r" (repeat), "r" (src->stream), "r" (src->size), "r" (dst->stream) \
    : "%r11", "%r12", "%r13", SIMD_CLOBBERED_REGS, "memory");
  roofline_rdtsc(c_high1, c_low1);
  out->ts_start = roofline_rdtsc_diff(c_high0, c_low0);
  out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
  out->bytes = (src->size+dst->size)*repeat;
  out->instructions = out->bytes/SIMD_BYTES;
}


