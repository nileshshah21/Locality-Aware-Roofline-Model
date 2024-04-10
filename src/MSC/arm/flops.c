#include "arm.h"
#include "../../list.h"

// Redefine these macros to fit NEON instruction formats for vector operations
#define roofline_flop_ins(op, reg) op " v" reg ".4s, v" reg ".4s, v" reg ".4s\n\t"

// Define NEON operations for ADD and MUL for simplicity. Extend as needed.
#define NEON_ADD "fadd"
#define NEON_MUL "fmul"
// For more complex operations like FMA, use appropriate NEON instructions or sequences.

#define simd_fp(op1, op2)           \
  roofline_flop_ins(op1, "0")       \
  roofline_flop_ins(op2, "1")       \
  roofline_flop_ins(op1, "2")       \
  roofline_flop_ins(op2, "3")       \
  roofline_flop_ins(op1, "4")       \
  roofline_flop_ins(op2, "5")       \
  roofline_flop_ins(op1, "6")       \
  roofline_flop_ins(op2, "7")       \
  roofline_flop_ins(op1, "8")       \
  roofline_flop_ins(op2, "9")       \
  roofline_flop_ins(op1, "10")      \
  roofline_flop_ins(op2, "11")      \
  roofline_flop_ins(op1, "12")      \
  roofline_flop_ins(op2, "13")      \
  roofline_flop_ins(op1, "14")      \
  roofline_flop_ins(op2, "15")

#define simd_fp_overhead(op1, op2) simd_fp(op1, op2) // Same as simd_fp for simplicity

#define NEON_SIMD_FP(op1, op2, regA, regB, regC, regD) \
    op1 " v" regA ".4s, v" regB ".4s, v" regC ".4s\n\t" \
    op2 " v" regD ".4s, v" regA ".4s, v" regB ".4s\n\t"

#define NEON_ASM_FLOPS(repeat, out, type_str, op1, op2) do { \
    roofline_output_begin_measure(out); \
    __asm__ __volatile__ ( \
        "1:\n\t" \
        NEON_SIMD_FP(op1, op2, "0", "1", "2", "3") \
        NEON_SIMD_FP(op1, op2, "4", "5", "6", "7") \
        "subs %0, %0, #1\n\t" \
        "bne 1b\n\t" \
        : "+r" (repeat) \
        : /* No inputs */ \
        : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory" \
    ); \
    roofline_output_end_measure(out, 0, 16 * 4 * repeat, repeat * 16); \
} while(0)

#define NEON_ASM_FLOPS_OVERHEAD(repeat, out, type_str, op1, op2) do { \
    roofline_output_begin_measure(out); \
    __asm__ __volatile__ ( \
        "1:\n\t" \
        "subs %0, %0, #1\n\t" \
        "bne 1b\n\t" \
        : "+r" (repeat) \
        : /* No inputs */ \
        : "memory" \
    ); \
    roofline_output_end_measure(out, 0, 0, 0); \
} while(0)

void benchmark_flops(long repeat, roofline_output out, int op_type) {
     
    // Assign NEON operations based on the operation type
    switch (op_type) {
    case ROOFLINE_ADD:
        NEON_ASM_FLOPS(repeat, out, "add", SIMD_ADD, SIMD_ADD);
        NEON_ASM_FLOPS_OVERHEAD(repeat, out, "add", SIMD_ADD, SIMD_ADD);
        break;
    case ROOFLINE_MUL:
        NEON_ASM_FLOPS(repeat, out, "mul", SIMD_MUL, SIMD_MUL);
        NEON_ASM_FLOPS_OVERHEAD(repeat, out, "mul", SIMD_MUL, SIMD_MUL);
        break;
    // Define other cases as necessary
    default:
        ERR("Not supported type %s\n", roofline_type_str(op_type));
        return;
    }

    // roofline_output_begin_measure(out);
    // // Perform the floating-point operations in a loop for the specified number of repetitions
    // __asm__ __volatile__ (
    //     "1:\n\t"
    //     simd_fp(op1, op2)
    //     "subs %0, %0, #1\n\t"
    //     "bne 1b\n\t"
    //     : "=r"(repeat)
    //     : "0"(repeat)
    //     : "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15" // Clobber list
    // );
    // roofline_output_end_measure(out, 0, 16 * 4 * repeat, 0); // Adjust as per the operation characteristics
}
