#include "../MSC.h"

// Assuming ARM NEON SIMD for ARM Cortex-A57
#if defined (__ARM_NEON__)
#define SIMD_REG           "q"        // NEON 128-bit SIMD register
#define SIMD_N_REGS        32         // Number of SIMD registers
#define SIMD_BYTES         16         // Bytes in the SIMD register
#define SIMD_FLOPS         4          // Number of floats in the vector register

// NEON does not have a direct equivalent of non-temporal stores/loads for all cases, 
// so using regular load and store instructions. Adjust based on specific needs.
#define SIMD_STORE_NT      "vst1q_f32"  // Adjust as needed for non-temporal hint
#define SIMD_LOAD_NT       "vld1q_f32"  // Adjust as needed for non-temporal hint
#define SIMD_STORE         "vst1q_f32"  // Instruction for store
#define SIMD_LOAD          "vld1q_f32"  // Instruction for load

// Arithmetic instructions
//#define SIMD_MUL           "vmulq_f32"  // Instruction for multiplication
//#define SIMD_ADD           "vaddq_f32"  // Instruction for addition
#define SIMD_MUL           "fmul"  // Example: Use actual AArch64 assembly instructions
#define SIMD_ADD           "fadd"
#endif

// NEON does not use a separate instruction for fused multiply-add; it's part of the standard multiply-add.
#define SIMD_FMA           "vmlaq_f32"  // Fused multiply add instruction

// Clobbered registers for NEON (example, adjust as needed)
#define SIMD_CLOBBERED_REG "q" // NEON register prefix
// Comma separated list of clobbered registers (asm); adjust based on actual usage and register names
#define SIMD_CLOBBERED_REGS "%q0", "%q1", "%q2", "%q3", "%q4", "%q5", "%q6", "%q7", \
                            "%q8", "%q9", "%q10", "%q11", "%q12", "%q13", "%q14", "%q15", \
                            "%q16", "%q17", "%q18", "%q19", "%q20", "%q21", "%q22", "%q23", \
                            "%q24", "%q25", "%q26", "%q27", "%q28", "%q29", "%q30", "%q31"



// Register numbers are directly used as part of the register name in ARM assembly, so these defines remain the same

#define STRIDE_0 "0"
#define REG_0  "0"
#define REG_1  "1"
#define REG_2  "2"
#define REG_3  "3"
#define REG_4  "4"
#define REG_5  "5"
#define REG_6  "6"
#define REG_7  "7"
#define REG_8  "8"
#define REG_9  "9"
#define REG_10 "10"
#define REG_11 "11"
#define REG_12 "12"
#define REG_13 "13"
#define REG_14 "14"
#define REG_15 "15"

// Define strides based on SIMD_BYTES for NEON; adjust as necessary for your specific use case
// The CHUNK_SIZE and STRIDE_x definitions would also need to be adjusted based on your specific microbenchmarking setup and requirements
#if defined (__ARM_NEON__)
#define CHUNK_SIZE 512

#define STRIDE_1  "16"
#define STRIDE_2  "32"
#define STRIDE_3  "48"
#define STRIDE_4  "64"
#define STRIDE_5  "80"
#define STRIDE_6  "96"
#define STRIDE_7  "112"
#define STRIDE_8  "128"
#define STRIDE_9  "144"
#define STRIDE_10 "160"
#define STRIDE_11 "176"
#define STRIDE_12 "192"
#define STRIDE_13 "208"
#define STRIDE_14 "224"
#define STRIDE_15 "240"
#define STRIDE_16 "256"
#define STRIDE_17 "272"
#define STRIDE_18 "288"
#define STRIDE_19 "304"
#define STRIDE_20 "320"
#define STRIDE_21 "336"
#define STRIDE_22 "352"
#define STRIDE_23 "368"
#define STRIDE_24 "384"
#define STRIDE_25 "400"
#define STRIDE_26 "416"
#define STRIDE_27 "432"
#define STRIDE_28 "448"
#define STRIDE_29 "464"
#define STRIDE_30 "480"
#define STRIDE_31 "496"
#endif

// The zeroing and roofing macros would need to be adapted for NEON instructions and register names
#if defined (__ARM_NEON__)
#define zero_reg(regnum) "eor v" #regnum ".16b, v" #regnum ".16b, v" #regnum ".16b\n\t"
#endif


#if defined (__ARM_NEON__)
#define roofline_flop_ins(op, regnum)   op ".f32 q" regnum", q" regnum", q" regnum"\n\t"
#else
#define roofline_flop_ins(op, reg)   op " " regnum ", " regnum ", " regnum "\n\t"
#endif

#define zero_simd()				\
  __asm__ __volatile__ (			\
    zero_reg(0) "\n\t"			\
    zero_reg(1) "\n\t"			\
    zero_reg(2) "\n\t"			\
    zero_reg(3) "\n\t"			\
    zero_reg(4) "\n\t"			\
    zero_reg(5) "\n\t"			\
    zero_reg(6) "\n\t"			\
    zero_reg(7) "\n\t"			\
    zero_reg(8) "\n\t"			\
    zero_reg(9) "\n\t"			\
    zero_reg(10) "\n\t"			\
    zero_reg(11) "\n\t"			\
    zero_reg(12) "\n\t"			\
    zero_reg(13) "\n\t"			\
    zero_reg(14) "\n\t"			\
    zero_reg(15) "\n\t"			\
    :::SIMD_CLOBBERED_REGS)
    // zero_reg(REG_16) "\n\t"			\
    // zero_reg(REG_17) "\n\t"			\
    // zero_reg(REG_18) "\n\t"			\
    // zero_reg(REG_19) "\n\t"			\
    // zero_reg(REG_20) "\n\t"			\
    // zero_reg(REG_21) "\n\t"			\
    // zero_reg(REG_22) "\n\t"			\
    // zero_reg(REG_23) "\n\t"			\
    // zero_reg(REG_24) "\n\t"			\
    // zero_reg(REG_25) "\n\t"			\
    // zero_reg(REG_26) "\n\t"			\
    // zero_reg(REG_27) "\n\t"			\
    // zero_reg(REG_28) "\n\t"			\
    // zero_reg(REG_29) "\n\t"			\
    // zero_reg(REG_30) "\n\t"			\
    // zero_reg(REG_31) "\n\t"			\
    // :::SIMD_CLOBBERED_REGS)

// #define roofline_load_ins(stride,srcreg, regnum) SIMD_LOAD " "stride"("srcreg"),%%" SIMD_REG regnum"\n\t"
// #define roofline_loadnt_ins(stride,srcreg, regnum) SIMD_LOAD_NT " "stride"("srcreg"),%%" SIMD_REG regnum"\n\t"
// #define roofline_store_ins(stride,dstreg, regnum) SIMD_STORE " %%" SIMD_REG  regnum ", " stride "("dstreg")\n\t"
// #define roofline_storent_ins(stride,dstreg, regnum) SIMD_STORE_NT " %%" SIMD_REG  regnum ", " stride "("dstreg")\n\t"
// Assuming definitions for stride and register numbers are provided
#define roofline_load_ins(stride, srcreg, regnum) "ld1 {v" regnum ".16b}, [" srcreg "]\n\t"
#define roofline_store_ins(stride, dstreg, regnum) "st1 {v" regnum ".16b}, [" dstreg "]\n\t"


void benchmark_flops(long repeat, roofline_output out, int op_type);
void benchmark_mov(roofline_stream data, roofline_output out, long repeat, int op_type);
void benchmark_2ld1st(roofline_stream dst, roofline_stream src, roofline_output out, long repeat);
void benchmark_copy(roofline_stream dst, roofline_stream src, roofline_output out, long repeat);
