#define _POSIX_C_SOURCE 200809L

#if defined(_OPENMP)
#include <omp.h>
#endif
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include "../roofline.h"
#include "MSC.h"

/************************************************************** MACROS VAR ******************************************************/
#define SIMD_CLOBBERED_REG "xmm" /* Name of the clobbered registers (asm) */
#define SIMD_CLOBBERED_REGS "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13", "%xmm14", "%xmm15" /* Comma separated list of clobbered registers (asm) */

#if defined (__AVX512__)
#define SIMD_REG           "zmm"       /* Name of the vector(simd) register (asm) */
#define SIMD_N_REGS        32          /* Number of vector(simd) registers */
#define SIMD_BYTES         64          /* Number Bytes in the vector register */
#define SIMD_FLOPS         8           /* Number of double in the vector register */
#define SIMD_STORE_NT      "vmovntpd"  /* Instruction for non temporal store */
#define SIMD_LOAD_NT       "vmovntdqa" /* Instruction for non temporal load */
#define SIMD_STORE         "vmovapd"   /* Instruction for store */
#define SIMD_LOAD          "vmovapd"   /* Instruction for load */
#define SIMD_MUL           "vmulpd"    /* Instruction for multiplication */
#define SIMD_ADD           "vaddpd"    /* Instruction for addition */
#elif defined (__AVX2__)
#define SIMD_REG           "ymm"
#define SIMD_N_REGS        16
#define SIMD_BYTES         32
#define SIMD_FLOPS         4
#define SIMD_STORE_NT      "vmovntpd"
#define SIMD_LOAD_NT       "vmovntdqa"
#define SIMD_STORE         "vmovapd"
#define SIMD_LOAD          "vmovapd"
#define SIMD_MUL           "vmulpd"
#define SIMD_ADD           "vaddpd"
#elif defined (__AVX__)
#define SIMD_REG           "ymm"
#define SIMD_N_REGS        16
#define SIMD_BYTES         32
#define SIMD_FLOPS         4
#define SIMD_STORE_NT      "vmovntpd"
#define SIMD_LOAD_NT       "vmovapd"
#define SIMD_STORE         "vmovapd"
#define SIMD_LOAD          "vmovapd"
#define SIMD_MUL           "vmulpd"
#define SIMD_ADD           "vaddpd"
#elif defined (__SSE4_1__)
#define SIMD_REG           "xmm"
#define SIMD_N_REGS        16
#define SIMD_BYTES         16
#define SIMD_FLOPS         2
#define SIMD_STORE_NT      "movntpd"
#define SIMD_LOAD_NT       "movapd"
#define SIMD_STORE         "movapd"
#define SIMD_LOAD          "movapd"
#define SIMD_MUL           "mulpd"
#define SIMD_ADD           "addpd"
#elif defined (__SSE2__)
#define SIMD_REG           "xmm"
#define SIMD_N_REGS        16
#define SIMD_BYTES         16
#define SIMD_FLOPS         2
#define SIMD_STORE_NT      "movntpd"
#define SIMD_LOAD_NT       "movapd"
#define SIMD_STORE         "movapd"
#define SIMD_LOAD          "movapd"
#define SIMD_MUL           "mulpd"
#define SIMD_ADD           "addpd"
#elif defined (__SSE__)
#define SIMD_REG           "xmm"
#define SIMD_N_REGS        16
#define SIMD_BYTES         16
#define SIMD_FLOPS         4
#define SIMD_STORE_NT      "movntps"
#define SIMD_LOAD_NT       "movaps"
#define SIMD_STORE         "movaps"
#define SIMD_LOAD          "movaps"
#define SIMD_MUL           "mulps"
#define SIMD_ADD           "addps"
#endif

#if defined (__FMA__)
#define SIMD_FMA           "vfmadd132pd" /* Fuse multiply add instruction */
#endif /* __FMA__ */


/******************************** Flops loop instructions ***************************/
#if defined (__AVX__)  || defined (__AVX512__) || defined (__FMA__)
/* asm instruction for flops between operand a & b. Result stored into c*/
#define simd_fp(op, a, b, c) op " %%" SIMD_REG a ", %%" SIMD_REG b ", %%" SIMD_REG c "\n\t"
#else
/* asm instruction for flops between operand a & b. Result stored into b*/
#define simd_fp(op, a, b) op " %%" SIMD_REG a ", %%" SIMD_REG b "\n\t"
#endif

#if defined __AVX__ || defined __AVX512__
/* unroll flop instructions */
#define fadd					\
    simd_fp(SIMD_ADD, "0", "0", "0")		\
    simd_fp(SIMD_ADD, "1", "1", "1")		\
    simd_fp(SIMD_ADD, "2", "2", "2")		\
    simd_fp(SIMD_ADD, "3", "3", "3")		\
    simd_fp(SIMD_ADD, "4", "4", "4")		\
    simd_fp(SIMD_ADD, "5", "5", "5")		\
    simd_fp(SIMD_ADD, "6", "6", "6")		\
    simd_fp(SIMD_ADD, "7", "7", "7")		\
    simd_fp(SIMD_ADD, "8", "8", "8")		\
    simd_fp(SIMD_ADD, "9", "9", "9")		\
    simd_fp(SIMD_ADD, "10", "10", "10")		\
    simd_fp(SIMD_ADD, "11", "11", "11")		\
    simd_fp(SIMD_ADD, "12", "12", "12")		\
    simd_fp(SIMD_ADD, "13", "13", "13")		\
    simd_fp(SIMD_ADD, "14", "14", "14")		\
    simd_fp(SIMD_ADD, "15", "15", "15")

#define fmul					\
    simd_fp(SIMD_MUL, "0", "0", "0")		\
    simd_fp(SIMD_MUL, "1", "1", "1")		\
    simd_fp(SIMD_MUL, "2", "2", "2")		\
    simd_fp(SIMD_MUL, "3", "3", "3")		\
    simd_fp(SIMD_MUL, "4", "4", "4")		\
    simd_fp(SIMD_MUL, "5", "5", "5")		\
    simd_fp(SIMD_MUL, "6", "6", "6")		\
    simd_fp(SIMD_MUL, "7", "7", "7")		\
    simd_fp(SIMD_MUL, "8", "8", "8")		\
    simd_fp(SIMD_MUL, "9", "9", "9")		\
    simd_fp(SIMD_MUL, "10", "10", "10")		\
    simd_fp(SIMD_MUL, "11", "11", "11")		\
    simd_fp(SIMD_MUL, "12", "12", "12")		\
    simd_fp(SIMD_MUL, "13", "13", "13")		\
    simd_fp(SIMD_MUL, "14", "14", "14")		\
    simd_fp(SIMD_MUL, "15", "15", "15")

#define fmad					\
    simd_fp(SIMD_ADD, "0", "0", "0")		\
    simd_fp(SIMD_MUL, "1", "1", "1")		\
    simd_fp(SIMD_ADD, "2", "2", "2")		\
    simd_fp(SIMD_MUL, "3", "3", "3")		\
    simd_fp(SIMD_ADD, "4", "4", "4")		\
    simd_fp(SIMD_MUL, "5", "5", "5")		\
    simd_fp(SIMD_ADD, "6", "6", "6")		\
    simd_fp(SIMD_MUL, "7", "7", "7")		\
    simd_fp(SIMD_ADD, "8", "8", "8")		\
    simd_fp(SIMD_MUL, "9", "9", "9")		\
    simd_fp(SIMD_ADD, "10", "10", "10")		\
    simd_fp(SIMD_MUL, "11", "11", "11")		\
    simd_fp(SIMD_ADD, "12", "12", "12")		\
    simd_fp(SIMD_MUL, "13", "13", "13")		\
    simd_fp(SIMD_ADD, "14", "14", "14")		\
    simd_fp(SIMD_MUL, "15", "15", "15")

#else
#define fadd					\
    simd_fp(SIMD_ADD, "0", "0")			\
    simd_fp(SIMD_ADD, "1", "1")			\
    simd_fp(SIMD_ADD, "2", "2")			\
    simd_fp(SIMD_ADD, "3", "3")			\
    simd_fp(SIMD_ADD, "4", "4")			\
    simd_fp(SIMD_ADD, "5", "5")			\
    simd_fp(SIMD_ADD, "6", "6")			\
    simd_fp(SIMD_ADD, "7", "7")			\
    simd_fp(SIMD_ADD, "8", "8")			\
    simd_fp(SIMD_ADD, "9", "9")			\
    simd_fp(SIMD_ADD, "10", "10")		\
    simd_fp(SIMD_ADD, "11", "11")		\
    simd_fp(SIMD_ADD, "12", "12")		\
    simd_fp(SIMD_ADD, "13", "13")		\
    simd_fp(SIMD_ADD, "14", "14")		\
    simd_fp(SIMD_ADD, "15", "15")

#define fmul					\
    simd_fp(SIMD_MUL, "0", "0")			\
    simd_fp(SIMD_MUL, "1", "1")			\
    simd_fp(SIMD_MUL, "2", "2")			\
    simd_fp(SIMD_MUL, "3", "3")			\
    simd_fp(SIMD_MUL, "4", "4")			\
    simd_fp(SIMD_MUL, "5", "5")			\
    simd_fp(SIMD_MUL, "6", "6")			\
    simd_fp(SIMD_MUL, "7", "7")			\
    simd_fp(SIMD_MUL, "8", "8")			\
    simd_fp(SIMD_MUL, "9", "9")			\
    simd_fp(SIMD_MUL, "10", "10")		\
    simd_fp(SIMD_MUL, "11", "11")		\
    simd_fp(SIMD_MUL, "12", "12")		\
    simd_fp(SIMD_MUL, "13", "13")		\
    simd_fp(SIMD_MUL, "14", "14")		\
    simd_fp(SIMD_MUL, "15", "15")

#define fmad					\
    simd_fp(SIMD_ADD, "0", "0")			\
    simd_fp(SIMD_MUL, "1", "1")			\
    simd_fp(SIMD_ADD, "2", "2")			\
    simd_fp(SIMD_MUL, "3", "3")			\
    simd_fp(SIMD_ADD, "4", "4")			\
    simd_fp(SIMD_MUL, "5", "5")			\
    simd_fp(SIMD_ADD, "6", "6")			\
    simd_fp(SIMD_MUL, "7", "7")			\
    simd_fp(SIMD_ADD, "8", "8")			\
    simd_fp(SIMD_MUL, "9", "9")			\
    simd_fp(SIMD_ADD, "10", "10")		\
    simd_fp(SIMD_MUL, "11", "11")		\
    simd_fp(SIMD_ADD, "12", "12")		\
    simd_fp(SIMD_MUL, "13", "13")		\
    simd_fp(SIMD_ADD, "14", "14")		\
    simd_fp(SIMD_MUL, "15", "15")

#endif

#ifdef __FMA__
#undef fmad
#define fmad					\
    simd_fp(SIMD_FMA, "0", "1", "2")		\
    simd_fp(SIMD_FMA, "3", "4", "5")		\
    simd_fp(SIMD_FMA, "6", "7", "8")		\
    simd_fp(SIMD_FMA, "9", "10", "11")		\
    simd_fp(SIMD_FMA, "12", "13", "14")		\
    simd_fp(SIMD_FMA, "15", "0", "1")		\
    simd_fp(SIMD_FMA, "2", "3", "4")		\
    simd_fp(SIMD_FMA, "5", "6", "7")		\
    simd_fp(SIMD_FMA, "8", "9", "10")		\
    simd_fp(SIMD_FMA, "11", "12", "13")		\
    simd_fp(SIMD_FMA, "14", "15", "0")		\
    simd_fp(SIMD_FMA, "1", "2", "3")		\
    simd_fp(SIMD_FMA, "4", "5", "6")		\
    simd_fp(SIMD_FMA, "7", "8", "9")		\
    simd_fp(SIMD_FMA, "10", "11", "12")		\
    simd_fp(SIMD_FMA, "13", "14", "15")
#endif

#if defined(_OPENMP)

#define asm_flops_begin(type_str, c_high, c_low) do{			\
    _Pragma("omp parallel")						\
    {									\
    _Pragma("omp barrier")						\
    _Pragma("omp master")						\
    roofline_rdtsc(c_high, c_low);					\
    __asm__ __volatile__ (						\
    "loop_flops_"type_str"_repeat:\n\t"

#define asm_flops_end(in, out, type_str, c_high, c_low, c_high1, c_low1) \
    "sub $1, %0\n\t"							\
    "jnz loop_flops_"type_str"_repeat\n\t"				\
    :: "r" (in->loop_repeat)	: SIMD_CLOBBERED_REGS);			\
    _Pragma("omp barrier")						\
    _Pragma("omp master")						\
    {									\
	roofline_rdtsc(c_high1, c_low1);				\
	out->ts_start = roofline_rdtsc_diff(c_high, c_low);		\
	out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);		\
	out->instructions = omp_get_num_threads()*16*in->loop_repeat;	\
	out->flops = out->instructions * SIMD_FLOPS;			\
    }									\
}} while(0)

#else
       
#define asm_flops_begin(type_str, c_high, c_low) do{			\
    __asm__ __volatile__ (						\
    "CPUID\n\t"								\
    "RDTSC\n\t"								\
    "mov %%rdx, %0\n\t"							\
    "mov %%rax, %1\n\t"							\
    "loop_flops_"type_str"_repeat:\n\t"

#define asm_flops_end(in, out, type_str, c_high, c_low, c_high1, c_low1) \
    "sub $1, %4\n\t"							\
    "jnz loop_flops_"type_str"_repeat\n\t"				\
    "CPUID\n\t"								\
    "RDTSC\n\t"								\
    "movq %%rdx, %2\n\t"						\
    "movq %%rax, %3\n\t"						\
    : "=&r" (c_high), "=&r" (c_low), "=&r" (c_high1), "=&r" (c_low1)	\
	: "r" (in->loop_repeat) : "%rax", "%rbx", "%rcx", "%rdx", SIMD_CLOBBERED_REGS); \
    out->ts_start = roofline_rdtsc_diff(c_high, c_low);			\
    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);			\
    out->instructions = 16*in->loop_repeat;				\
    out->flops = out->instructions*SIMD_FLOPS;				\
    } while(0)
       
#endif

void fpeak_benchmark(const struct roofline_sample_in * in, struct roofline_sample_out * out, int type){
    volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
    switch(type){
    case ROOFLINE_ADD:
	asm_flops_begin("add", c_high, c_low) fadd asm_flops_end(in, out, "add", c_high, c_low, c_high1, c_low1);
	break;
    case ROOFLINE_MUL:
	asm_flops_begin("mul", c_high, c_low) fmul asm_flops_end(in, out, "mul", c_high, c_low, c_high1, c_low1);
	break;
    case ROOFLINE_MAD:
	asm_flops_begin("mad", c_high, c_low) fmad asm_flops_end(in, out, "mad", c_high, c_low, c_high1, c_low1);
#if defined (__FMA__)
	out->flops*=2;
#endif
	break;
    default:
	break;
    }
}

/******************************** Memory loop instructions ***************************/
/* memory instructions from/to vector unit to/from dstreg/srcreg + stride */
#define roofline_load_ins(stride,srcreg, regnum) SIMD_LOAD " "stride"("srcreg"),%%" SIMD_REG regnum"\n\t"
#define roofline_loadnt_ins(stride,srcreg, regnum) SIMD_LOAD_NT " "stride"("srcreg"),%%" SIMD_REG regnum"\n\t"
#define roofline_store_ins(stride,dstreg, regnum) SIMD_STORE " %%" SIMD_REG  regnum ", " stride "("dstreg")\n\t"
#define roofline_storent_ins(stride,dstreg, regnum) SIMD_STORE_NT " %%" SIMD_REG  regnum ", " stride "("dstreg")\n\t"

/*  unroll several instructions of above roofline_*_ins(parameter mem_uop) macro functions */
#if defined (__AVX512__)
#define SIMD_CHUNK_SIZE 1920

#define simd_mov(mem_uop, datareg)	\
    mem_uop("0",datareg,"0")		\
    mem_uop("64",datareg,"1")		\
    mem_uop("128",datareg,"2")		\
    mem_uop("192",datareg,"3")		\
    mem_uop("256",datareg,"4")		\
    mem_uop("320",datareg,"5")		\
    mem_uop("384",datareg,"6")		\
    mem_uop("448",datareg,"7")		\
    mem_uop("512",datareg,"8")		\
    mem_uop("576",datareg,"9")		\
    mem_uop("640",datareg,"10")		\
    mem_uop("704",datareg,"11")		\
    mem_uop("768",datareg,"12")		\
    mem_uop("832",datareg,"13")		\
    mem_uop("896",datareg,"14")		\
    mem_uop("960",datareg,"15")		\
    mem_uop("1024",datareg,"16")	\
    mem_uop("1088",datareg,"17")	\
    mem_uop("1152",datareg,"18")	\
    mem_uop("1216",datareg,"19")	\
    mem_uop("1280",datareg,"20")	\
    mem_uop("1344",datareg,"21")	\
    mem_uop("1408",datareg,"22")	\
    mem_uop("1472",datareg,"23")	\
    mem_uop("1536",datareg,"24")	\
    mem_uop("1600",datareg,"25")	\
    mem_uop("1664",datareg,"26")	\
    mem_uop("1728",datareg,"27")	\
    mem_uop("1792",datareg,"28")	\
    mem_uop("1856",datareg,"29")

#define simd_2ld1st(reg)			\
    roofline_load_ins("0",reg,"0")		\
    roofline_load_ins("64",reg,"1")		\
    roofline_store_ins("128",reg,"2")		\
    roofline_load_ins("192",reg,"3")		\
    roofline_load_ins("256",reg,"4")		\
    roofline_store_ins("320",reg,"5")		\
    roofline_load_ins("384",reg,"6")		\
    roofline_load_ins("448",reg,"7")		\
    roofline_store_ins("512",reg,"8")		\
    roofline_load_ins("576",reg,"9")		\
    roofline_load_ins("640",reg,"10")		\
    roofline_store_ins("704",reg,"11")		\
    roofline_load_ins("768",reg,"12")		\
    roofline_load_ins("832",reg,"13")		\
    roofline_store_ins("896",reg,"14")		\
    roofline_load_ins("960",reg,"15")		\
    roofline_load_ins("1024",reg,"16")		\
    roofline_store_ins("1088",reg,"17")		\
    roofline_load_ins("1152",reg,"18")		\
    roofline_load_ins("1216",reg,"19")		\
    roofline_store_ins("1280",reg,"20")		\
    roofline_load_ins("1344",reg,"21")		\
    roofline_load_ins("1408",reg,"22")		\
    roofline_store_ins("1472",reg,"23")		\
    roofline_load_ins("1536",reg,"24")		\
    roofline_load_ins("1600",reg,"25")		\
    roofline_store_ins("1664",reg,"26")		\
    roofline_load_ins("1728",reg,"27")		\
    roofline_load_ins("1792",reg,"28")		\
    roofline_store_ins("1856",reg,"29")

#define simd_copy(reg)				\
    roofline_load_ins("0",reg,"0")		\
    roofline_store_ins("64",reg,"1")		\
    roofline_load_ins("128",reg,"2")		\
    roofline_store_ins("192",reg,"3")		\
    roofline_load_ins("256",reg,"4")		\
    roofline_store_ins("320",reg,"5")		\
    roofline_load_ins("384",reg,"6")		\
    roofline_store_ins("448",reg,"7")		\
    roofline_load_ins("512",reg,"8")		\
    roofline_store_ins("576",reg,"9")		\
    roofline_load_ins("640",reg,"10")		\
    roofline_store_ins("704",reg,"11")		\
    roofline_load_ins("768",reg,"12")		\
    roofline_store_ins("832",reg,"13")		\
    roofline_load_ins("896",reg,"14")		\
    roofline_store_ins("960",reg,"15")		\
    roofline_load_ins("1024",reg,"16")		\
    roofline_store_ins("1088",reg,"17")		\
    roofline_load_ins("1152",reg,"18")		\
    roofline_store_ins("1216",reg,"19")		\
    roofline_load_ins("1280",reg,"20")		\
    roofline_store_ins("1344",reg,"21")		\
    roofline_load_ins("1408",reg,"22")		\
    roofline_store_ins("1472",reg,"23")		\
    roofline_load_ins("1536",reg,"24")		\
    roofline_store_ins("1600",reg,"25")		\
    roofline_load_ins("1664",reg,"26")		\
    roofline_store_ins("1728",reg,"27")		\
    roofline_load_ins("1792",reg,"28")		\
    roofline_store_ins("1856",reg,"29")

#elif defined (__AVX__)
#define SIMD_CHUNK_SIZE 384
#define simd_mov(mem_uop, datareg)	\
    mem_uop("0",datareg,"0")		\
    mem_uop("32",datareg,"1")		\
    mem_uop("64",datareg,"2")		\
    mem_uop("96",datareg,"3")		\
    mem_uop("128",datareg,"4")		\
    mem_uop("160",datareg,"5")		\
    mem_uop("192",datareg,"6")		\
    mem_uop("224",datareg,"7")		\
    mem_uop("256",datareg,"8")		\
    mem_uop("288",datareg,"9")		\
    mem_uop("320",datareg,"10")		\
    mem_uop("352",datareg,"11")

#define simd_2ld1st(reg)			\
    roofline_load_ins("0",reg,"0")		\
    roofline_load_ins("32",reg,"1")		\
    roofline_store_ins("64",reg,"2")		\
    roofline_load_ins("96",reg,"3")		\
    roofline_load_ins("128",reg,"4")		\
    roofline_store_ins("160",reg,"5")		\
    roofline_load_ins("192",reg,"6")		\
    roofline_load_ins("224",reg,"7")		\
    roofline_store_ins("256",reg,"8")		\
    roofline_load_ins("288",reg,"9")		\
    roofline_load_ins("320",reg,"10")		\
    roofline_store_ins("352",reg,"11")

#define simd_copy(reg)				\
    roofline_load_ins("0",reg,"0")		\
    roofline_store_ins("32",reg,"1")		\
    roofline_load_ins("64",reg,"2")		\
    roofline_store_ins("96",reg,"3")		\
    roofline_load_ins("128",reg,"4")		\
    roofline_store_ins("160",reg,"5")		\
    roofline_load_ins("192",reg,"6")		\
    roofline_store_ins("224",reg,"7")		\
    roofline_load_ins("256",reg,"8")		\
    roofline_store_ins("288",reg,"9")		\
    roofline_load_ins("320",reg,"10")		\
    roofline_store_ins("352",reg,"11")

#elif defined (__SSE__) || defined (__SSE2__) || defined (__SSE4_1__)
#define SIMD_CHUNK_SIZE 192
#define simd_mov(mem_uop, datareg)	\
    mem_uop("0",datareg,"0")		\
    mem_uop("16",datareg,"1")		\
    mem_uop("32",datareg,"2")		\
    mem_uop("48",datareg,"3")		\
    mem_uop("64",datareg,"4")		\
    mem_uop("80",datareg,"5")		\
    mem_uop("96",datareg,"6")		\
    mem_uop("112",datareg,"7")		\
    mem_uop("128",datareg,"8")		\
    mem_uop("144",datareg,"9")		\
    mem_uop("160",datareg,"10")		\
    mem_uop("176",datareg,"11")

#define simd_2ld1st(reg)			\
    roofline_load_ins("0",reg,"0")		\
    roofline_load_ins("16",reg,"1")		\
    roofline_store_ins("32",reg,"2")		\
    roofline_load_ins("48",reg,"3")		\
    roofline_load_ins("64",reg,"4")		\
    roofline_store_ins("80",reg,"5")		\
    roofline_load_ins("96",reg,"6")		\
    roofline_load_ins("112",reg,"7")		\
    roofline_store_ins("128",reg,"8")		\
    roofline_load_ins("144",reg,"9")		\
    roofline_load_ins("160",reg,"10")		\
    roofline_store_ins("176",reg,"11")

#define simd_copy(reg)				\
    roofline_load_ins("0",reg,"0")		\
    roofline_store_ins("16",reg,"1")		\
    roofline_load_ins("32",reg,"2")		\
    roofline_store_ins("48",reg,"3")		\
    roofline_load_ins("64",reg,"4")		\
    roofline_store_ins("80",reg,"5")		\
    roofline_load_ins("96",reg,"6")		\
    roofline_store_ins("112",reg,"7")		\
    roofline_load_ins("128",reg,"8")		\
    roofline_store_ins("144",reg,"9")		\
    roofline_load_ins("160",reg,"10")		\
    roofline_store_ins("176",reg,"11")

#endif

#define reg_mv "%%r11"

size_t chunk_size = SIMD_CHUNK_SIZE; /* default chunk_size */
#define roofline_load_ins_loop simd_mov(roofline_load_ins, reg_mv)
#define roofline_loadnt_ins_loop simd_mov(roofline_loadnt_ins, reg_mv)
#define roofline_store_ins_loop simd_mov(roofline_store_ins, reg_mv)
#define roofline_storent_ins_loop simd_mov(roofline_storent_ins, reg_mv)
#define roofline_2ld1st_ins_loop simd_2ld1st(reg_mv)
#define roofline_copy_ins_loop simd_copy(reg_mv)

#define bandwidth_asm_begin(type_name)				\
    __asm__ __volatile__ (					\
			  "loop_"type_name"_repeat:\n\t"	\
			  "mov %1, "reg_mv"\n\t"		\
			  "mov %2, %%r12\n\t"			\
			  "buffer_"type_name"_increment:\n\t"


#define bandwidth_asm_end(type_name, repeat, stream, size)		\
    "add $"roofline_stringify(SIMD_CHUNK_SIZE)", "reg_mv"\n\t"		\
					      "sub $"roofline_stringify(SIMD_CHUNK_SIZE)", %%r12\n\t" \
											"jnz buffer_"type_name"_increment\n\t" \
											"sub $1, %0\n\t" \
											"jnz loop_"type_name"_repeat\n\t" \
											"CPUID\n\t" \
											:: "r" (repeat), "r" (stream), "r" (size) \
	: "%r11", "%r12", SIMD_CLOBBERED_REGS, "memory")

/**********************************************************************************************************************************/

void bandwidth_benchmark(const struct roofline_sample_in * in, struct roofline_sample_out * out, int type){
    uint64_t c_low0=0, c_low1=0, c_high0=0, c_high1=0;
    chunk_size = SIMD_CHUNK_SIZE;
    ROOFLINE_STREAM_TYPE * stream = in->stream;
    size_t size = in->stream_size;
#if defined(_OPENMP)
#pragma omp parallel
#pragma omp single
	size /= omp_get_num_threads();;

#pragma omp parallel firstprivate(stream)
    {
	stream = stream + omp_get_thread_num()*size/sizeof(*stream);
#pragma omp barrier
#pragma omp master
	roofline_rdtsc(c_high0, c_low0);
	switch(type){
	case ROOFLINE_LOAD:
	    bandwidth_asm_begin("load") roofline_load_ins_loop bandwidth_asm_end("load", in->loop_repeat, stream, size);
	    break;
	case ROOFLINE_LOAD_NT:
	    bandwidth_asm_begin("loadnt") roofline_loadnt_ins_loop bandwidth_asm_end("loadnt", in->loop_repeat, stream, size);
	    break;
	case ROOFLINE_STORE:
	    bandwidth_asm_begin("store") roofline_store_ins_loop bandwidth_asm_end("store", in->loop_repeat, stream, size);
	    break;
	case ROOFLINE_STORE_NT:
	    bandwidth_asm_begin("storent") roofline_storent_ins_loop bandwidth_asm_end("storent", in->loop_repeat, stream, size);
	    break;
	case ROOFLINE_2LD1ST:
	    bandwidth_asm_begin("2ld1st") roofline_2ld1st_ins_loop bandwidth_asm_end("2ld1st", in->loop_repeat, stream, size);
	case ROOFLINE_COPY:
	    bandwidth_asm_begin("copy") roofline_copy_ins_loop bandwidth_asm_end("copy", in->loop_repeat, stream, size);
	    break;
	default:
	    break;
	}
#pragma omp barrier
#pragma omp master
	roofline_rdtsc(c_high1, c_low1);
    }
#else
	switch(type){
	case ROOFLINE_LOAD:
	    roofline_rdtsc(c_high0, c_low0);
	    bandwidth_asm_begin("load") roofline_load_ins_loop bandwidth_asm_end("load", in->loop_repeat, stream, size);
	    roofline_rdtsc(c_high1, c_low1);
	    break;
	case ROOFLINE_LOAD_NT:
	    roofline_rdtsc(c_high0, c_low0);
	    bandwidth_asm_begin("loadnt") roofline_loadnt_ins_loop bandwidth_asm_end("loadnt", in->loop_repeat, stream, size);
	    roofline_rdtsc(c_high1, c_low1);
	    break;
	case ROOFLINE_STORE:
	    roofline_rdtsc(c_high0, c_low0);
	    bandwidth_asm_begin("store") roofline_store_ins_loop bandwidth_asm_end("store", in->loop_repeat, stream, size);
	    roofline_rdtsc(c_high1, c_low1);
	    break;
	case ROOFLINE_STORE_NT:
	    roofline_rdtsc(c_high0, c_low0);
	    bandwidth_asm_begin("storent") roofline_storent_ins_loop bandwidth_asm_end("storent", in->loop_repeat, stream, size);
	    roofline_rdtsc(c_high1, c_low1);
	    break;
	case ROOFLINE_2LD1ST:
	    roofline_rdtsc(c_high0, c_low0);
	    bandwidth_asm_begin("2ld1st") roofline_2ld1st_ins_loop bandwidth_asm_end("2ld1st", in->loop_repeat, stream, size);
	    roofline_rdtsc(c_high1, c_low1);
	case ROOFLINE_COPY:
	    roofline_rdtsc(c_high0, c_low0);
	    bandwidth_asm_begin("copy") roofline_copy_ins_loop bandwidth_asm_end("copy", in->loop_repeat, stream, size);
	    roofline_rdtsc(c_high1, c_low1);
	    break;
	default:
	    break;
	}
#endif
    	out->ts_start = roofline_rdtsc_diff(c_high0, c_low0);
    	out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
    	out->bytes = in->stream_size*in->loop_repeat;
    	out->instructions = out->bytes/SIMD_BYTES;
}

/***************************************** OI BENCHMARKS GENERATION ******************************************/

#include <stdio.h>

#if defined (__AVX__)  || defined (__AVX512__)
static void dprint_FUOP(int fd, const char * op, unsigned * regnum){
    dprintf(fd, "\"%s %%%%%s%d, %%%%%s%d, %%%%%s%d\\n\\t\"\\\n",
	    op, SIMD_REG, *regnum, SIMD_REG, *regnum, SIMD_REG, *regnum);
    *regnum = (*regnum+1)%SIMD_N_REGS;
}
#else 
static void dprint_FUOP(int fd, const char * op, unsigned * regnum){
    dprintf(fd, "\"%s %%%%%s%d, %%%%%s%d\\n\\t\"\\\n", op, SIMD_REG, *regnum, SIMD_REG, *regnum);
    *regnum = (*regnum+1)%SIMD_N_REGS;
}
#endif

static void dprint_MUOP(int fd, int type, int i,  off_t * offset, unsigned * regnum, const char * datareg){
    switch(type){
    case ROOFLINE_LOAD:
	dprintf(fd, "\"%s %lu(%%%%%s), %%%%%s%d\\n\\t\"\\\n", SIMD_LOAD, *offset, datareg, SIMD_REG, *regnum);
	break;
    case ROOFLINE_LOAD_NT:
	dprintf(fd, "\"%s %lu(%%%%%s), %%%%%s%d\\n\\t\"\\\n", SIMD_LOAD_NT, *offset, datareg, SIMD_REG, *regnum);
	break;
    case ROOFLINE_STORE:
	dprintf(fd, "\"%s %%%%%s%d, %lu(%%%%%s)\\n\\t\"\\\n", SIMD_STORE, SIMD_REG, *regnum, *offset, datareg);
	break;
    case ROOFLINE_STORE_NT:
	dprintf(fd, "\"%s %%%%%s%d, %lu(%%%%%s)\\n\\t\"\\\n", SIMD_STORE_NT, SIMD_REG, *regnum, *offset, datareg);
	break;
    case ROOFLINE_2LD1ST:
	if(i%3) dprint_MUOP(fd,ROOFLINE_LOAD,i,offset,regnum,datareg);
	else dprint_MUOP(fd,ROOFLINE_STORE,i,offset,regnum,datareg);
	break;
    case ROOFLINE_COPY:
	if(i%2) dprint_MUOP(fd,ROOFLINE_LOAD,i,offset,regnum,datareg);
	else dprint_MUOP(fd,ROOFLINE_STORE,i,offset,regnum,datareg);
	break;
    default:
	break;
    }
    *offset+=SIMD_BYTES;
    *regnum = (*regnum+1)%SIMD_N_REGS;
}

static void  dprint_header(int fd) {
    dprintf(fd, "#include <stdlib.h>\n");
    dprintf(fd, "#if defined(_OPENMP)\n");
    dprintf(fd, "#include <omp.h>\n");
    dprintf(fd, "#endif\n");
    dprintf(fd, "#include \"roofline.h\"\n\n");
    dprintf(fd, "#define rdtsc(c_high,c_low) ");
    dprintf(fd, "__asm__ __volatile__ (\"CPUID\\n\\tRDTSC\\n\\tmovq %%%%rdx, %%0\\n\\tmovq %%%%rax, %%1\\n\\t\"");
    dprintf(fd, ":\"=r\" (c_high), \"=r\" (c_low)::\"%%rax\", \"%%rbx\", \"%%rcx\", \"%%rdx\")\n");
}
 
static void dprint_oi_bench_begin(int fd, const char * id, const char * name){
    dprintf(fd, "void %s(struct roofline_sample_in * in, struct roofline_sample_out * out, __attribute__ ((unused)) int type){\n", name);
    dprintf(fd, "volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;\n");
    dprintf(fd, "%s * stream = in->stream;\n",roofline_stringify(ROOFLINE_STREAM_TYPE));
    dprintf(fd, "size_t size = in->stream_size;\n");
    dprintf(fd,"#if defined(_OPENMP)\n");
    dprintf(fd,"#pragma omp parallel firstprivate(size, stream)\n{\n");
    dprintf(fd,"size /= omp_get_num_threads();\n");
    dprintf(fd,"stream += omp_get_thread_num()*size/sizeof(*stream);\n");
    dprintf(fd,"#pragma omp barrier\n");
    dprintf(fd,"#pragma omp master\n");
    dprintf(fd, "#endif\n");
    dprintf(fd,"rdtsc(c_high,c_low);\n");
    dprintf(fd, "__asm__ __volatile__ (\\\n");
    dprintf(fd, "\"loop_%s_repeat:\\n\\t\"\\\n", id);
    dprintf(fd, "\"mov %%1, %%%%r11\\n\\t\"\\\n");
    dprintf(fd, "\"mov %%2, %%%%r12\\n\\t\"\\\n");
    dprintf(fd, "\"buffer_%s_increment:\\n\\t\"\\\n", id);
}

static void dprint_oi_bench_end(int fd, const char * id, off_t offset){
    dprintf(fd,"\"add $%lu, %%%%r11\\n\\t\"\\\n", offset);
    dprintf(fd,"\"sub $%lu, %%%%r12\\n\\t\"\\\n", offset);
    dprintf(fd,"\"jnz buffer_%s_increment\\n\\t\"\\\n", id);
    dprintf(fd,"\"sub $1, %%0\\n\\t\"\\\n");
    dprintf(fd,"\"jnz loop_%s_repeat\\n\\t\"\\\n", id);
    dprintf(fd,":: \"r\" (in->loop_repeat), \"r\" (stream), \"r\" (size)\\\n");
    dprintf(fd,": \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"memory\" );\n", SIMD_CLOBBERED_REGS);
    dprintf(fd, "#if defined(_OPENMP)\n");
    dprintf(fd,"#pragma omp barrier\n");
    dprintf(fd,"#pragma omp master\n");
    dprintf(fd, "#endif\n");
    dprintf(fd,"rdtsc(c_high1,c_low1);\n");
    dprintf(fd, "#if defined(_OPENMP)\n");
    dprintf(fd,"}\n");
    dprintf(fd, "#endif\n");
    dprintf(fd, "out->ts_end = ((c_high1 << 32) | c_low1);\n");
    dprintf(fd, "out->ts_start = ((c_high << 32) | c_low);\n");
}

static int roofline_compile_lib(char * c_path, char* so_path){
    char cmd[2048];
    memset(cmd,0,sizeof(cmd));
    sprintf(cmd, "%s -fPIC -shared %s -rdynamic -o %s %s", compiler, c_path, so_path, omp_flag);
    return system(cmd);
}

off_t roofline_benchmark_write_oi_bench(int fd, const char * name, int type, double oi){
    off_t offset;
    size_t len;
    unsigned i, regnum, mem_instructions = 0, fop_instructions = 0, fop_per_mop, mop_per_fop;
    char * idx;
    if(oi<=0)
	return 0;
    len = 1+strlen(roofline_type_str(type));
    idx = malloc(len);
    memset(idx,0,len);
    snprintf(idx,len,"%s", roofline_type_str(type));
    regnum=0;           /* SIMD register number */
    offset = 0;         /* the offset of each load/store instruction */
    mem_instructions=0; /* The number of printed memory instructions */
    fop_instructions=0; /* The number of printed flop instructions */
    fop_per_mop = oi * SIMD_BYTES / SIMD_FLOPS;
    mop_per_fop = SIMD_FLOPS / (oi * SIMD_BYTES);
    dprint_oi_bench_begin(fd, idx, name);
    if(mop_per_fop == 1){
	unsigned ppcm = SIMD_N_REGS/2;
	if(type == ROOFLINE_2LD1ST){ppcm = roofline_PPCM(SIMD_N_REGS,3);}
	for(i=0;i<ppcm;i++){
	    dprint_MUOP(fd, type, i, &offset, &regnum, "r11");
            if(i%2){dprint_FUOP(fd, SIMD_MUL, &regnum);}
	    else{   dprint_FUOP(fd, SIMD_ADD, &regnum);}
	}
	mem_instructions = fop_instructions = ppcm;
    }
    else if(mop_per_fop > 1){
	fop_instructions = SIMD_N_REGS;
	mem_instructions = fop_instructions * mop_per_fop;
	if(type == ROOFLINE_2LD1ST){mem_instructions = roofline_PPCM(mem_instructions,3);}
	fop_instructions = mem_instructions / mop_per_fop;
	for(i=0;i<mem_instructions;i++){
	    dprint_MUOP(fd, type, i, &offset, &regnum, "r11");
	    if(i%mop_per_fop==0){
		if((i/mop_per_fop)%2){dprint_FUOP(fd, SIMD_MUL, &regnum);}
		else{dprint_FUOP(fd, SIMD_ADD, &regnum);}
	    }
	}
    }
    else if(fop_per_mop > 1){
        mem_instructions = SIMD_N_REGS;
	fop_instructions = mem_instructions * fop_per_mop;
	if(type == ROOFLINE_2LD1ST){
	    mem_instructions = roofline_PPCM(mem_instructions, 3);
	    fop_instructions = mem_instructions*fop_per_mop;
	}
	for(i=0;i<fop_instructions;i++){
	    if(i%fop_per_mop == 0) {
	        dprint_MUOP(fd, type, i, &offset, &regnum, "r11");
	    }
	    if(i%2){dprint_FUOP(fd, SIMD_MUL, &regnum);}
	    else{dprint_FUOP(fd, SIMD_ADD, &regnum);}
	}
    }
    dprint_oi_bench_end(fd, idx, offset);
    dprintf(fd, "out->instructions = in->loop_repeat * %u * in->stream_size / %lu;\n", mem_instructions+fop_instructions, offset);
    dprintf(fd, "out->flops = in->loop_repeat * %u * in->stream_size / %lu;\n", fop_instructions*SIMD_FLOPS, offset);
    dprintf(fd, "out->bytes = in->loop_repeat * in->stream_size;\n");
    dprintf(fd, "}\n\n");
    free(idx);
    return offset;
}

static void (* roofline_load_lib(const char * path, const char * funname)) (const struct roofline_sample_in *, struct roofline_sample_out *, int)
{
    void * benchmark, * handle;
    
    dlerror();
    handle = dlopen(path, RTLD_NOW|RTLD_DEEPBIND);
    if(handle == NULL){
	fprintf(stderr, "Failed to load dynamic library %s: %s\n", path, dlerror());
	return NULL;
    }

    dlerror();
    benchmark = dlsym(handle, funname);
    if(benchmark == NULL){
	fprintf(stderr, "Failed to load function %s from %s: %s\n", funname, path, dlerror());
    }
    return (void (*)(const struct roofline_sample_in *, struct roofline_sample_out *, int))benchmark;
}

void (* roofline_oi_bench(const double oi, const int type))(const struct roofline_sample_in * in, struct roofline_sample_out * out, int type)
{
    const char * func_name = "roofline_oi_benchmark";

    char tempname[1024], * c_path, * so_path;
    size_t len;
    int fd; 
    void (* benchmark) (const struct roofline_sample_in *, struct roofline_sample_out *, int);

    if(oi<=0){
	fprintf(stderr, "operational intensity must be greater than 0.\n");
	return NULL;
    }


    /* Create the filenames */
    snprintf(tempname, sizeof(tempname), "roofline_benchmark_%s_oi_%lf", roofline_type_str(type), oi);
    len = strlen(tempname)+3; c_path =  malloc(len); memset(c_path,  0, len); snprintf(c_path, len, "%s.c", tempname);
    len = strlen(tempname)+4; so_path = malloc(len); memset(so_path, 0, len); snprintf(so_path, len, "%s.so", tempname);
    
    /* Generate benchmark */
    fd = open(c_path, O_WRONLY|O_CREAT, 0644);
    /* Write the appropriate roofline to the file */
    dprint_header(fd);
    chunk_size = roofline_benchmark_write_oi_bench(fd, func_name, type, oi);
    /* Compile the roofline function */
    close(fd); 
    roofline_compile_lib(c_path, so_path);
    /* Load the roofline function */
    benchmark = roofline_load_lib(so_path, func_name);

    unlink(c_path);
    unlink(so_path);
    free(c_path);
    free(so_path);
    return benchmark;
}

int benchmark_types_supported(){
    return ROOFLINE_LOAD|ROOFLINE_LOAD_NT|ROOFLINE_STORE|ROOFLINE_STORE_NT|ROOFLINE_2LD1ST|ROOFLINE_COPY|ROOFLINE_MUL|ROOFLINE_ADD|ROOFLINE_MAD;
}

