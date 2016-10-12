#include <stdio.h>
#include "../roofline.h"
#include "MSC.h"

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

#define SIMD_FMA "vfmadd132pd" /* Fuse multiply add instruction */


#define SIMD_CLOBBERED_REG "xmm" /* Name of the clobbered registers (asm) */
#define SIMD_CLOBBERED_REGS "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13", "%xmm14", "%xmm15" /* Comma separated list of clobbered registers (asm) */

#if defined (__AVX__) || defined (__AVX2__) || defined (__AVX512__)

/* asm instruction for flops between operand a & b. Result stored into c*/
#define simd_fp(op1, op2)						\
  op1 " %%" SIMD_REG "0, %%" SIMD_REG "0, %%" SIMD_REG "0\n\t"		\
  op2 " %%" SIMD_REG "1, %%" SIMD_REG "1, %%" SIMD_REG "1\n\t"		\
  op1 " %%" SIMD_REG "2, %%" SIMD_REG "2, %%" SIMD_REG "2\n\t"		\
  op2 " %%" SIMD_REG "3, %%" SIMD_REG "3, %%" SIMD_REG "3\n\t"		\
  op1 " %%" SIMD_REG "4, %%" SIMD_REG "4, %%" SIMD_REG "4\n\t"		\
  op2 " %%" SIMD_REG "5, %%" SIMD_REG "5, %%" SIMD_REG "5\n\t"		\
  op1 " %%" SIMD_REG "6, %%" SIMD_REG "6, %%" SIMD_REG "6\n\t"		\
  op2 " %%" SIMD_REG "7, %%" SIMD_REG "7, %%" SIMD_REG "7\n\t"		\
  op1 " %%" SIMD_REG "8, %%" SIMD_REG "8, %%" SIMD_REG "8\n\t"		\
  op2 " %%" SIMD_REG "9, %%" SIMD_REG "9, %%" SIMD_REG "9\n\t"		\
  op1 " %%" SIMD_REG "10, %%" SIMD_REG "10, %%" SIMD_REG "10\n\t"	\
  op2 " %%" SIMD_REG "11, %%" SIMD_REG "11, %%" SIMD_REG "11\n\t"	\
  op1 " %%" SIMD_REG "12, %%" SIMD_REG "12, %%" SIMD_REG "12\n\t"	\
  op2 " %%" SIMD_REG "13, %%" SIMD_REG "13, %%" SIMD_REG "13\n\t"	\
  op1 " %%" SIMD_REG "14, %%" SIMD_REG "14, %%" SIMD_REG "14\n\t"	\
  op2 " %%" SIMD_REG "15, %%" SIMD_REG "15, %%" SIMD_REG "15\n\t"

#else
/* asm instruction for flops between operand a & b. Result stored into b*/
#define simd_fp(op1, op2)			\
  op1 " %%" SIMD_REG"0, %%" SIMD_REG"0\n\t"	\
  op2 " %%" SIMD_REG"1, %%" SIMD_REG"1\n\t"	\
  op1 " %%" SIMD_REG"2, %%" SIMD_REG"2\n\t"	\
  op2 " %%" SIMD_REG"3, %%" SIMD_REG"3\n\t"	\
  op1 " %%" SIMD_REG"4, %%" SIMD_REG"4\n\t"	\
  op2 " %%" SIMD_REG"5, %%" SIMD_REG"5\n\t"	\
  op1 " %%" SIMD_REG"6, %%" SIMD_REG"6\n\t"	\
  op2 " %%" SIMD_REG"7, %%" SIMD_REG"7\n\t"	\
  op1 " %%" SIMD_REG"8, %%" SIMD_REG"8\n\t"	\
  op2 " %%" SIMD_REG"9, %%" SIMD_REG"9\n\t"	\
  op1 " %%" SIMD_REG"10, %%" SIMD_REG"10\n\t"	\
  op2 " %%" SIMD_REG"11, %%" SIMD_REG"11\n\t"	\
  op1 " %%" SIMD_REG"12, %%" SIMD_REG"12\n\t"	\
  op2 " %%" SIMD_REG"13, %%" SIMD_REG"13\n\t"	\
  op1 " %%" SIMD_REG"14, %%" SIMD_REG"14\n\t"	\
  op2 " %%" SIMD_REG"15, %%" SIMD_REG"15\n\t"  
#endif

#if defined (__AVX512__)
#define zero_reg(regnum) "vpxorq %%" SIMD_REG regnum ", %%" SIMD_REG regnum ",%%" SIMD_REG regnum "\n\t"
#elif defined(__AVX2__)
#define zero_reg(regnum) "vpxor %%ymm" regnum ", %%ymm" regnum ",%%ymm" regnum "\n\t"
#elif defined (__AVX__) || defined (__SSE__) || defined (__SSE4_1__) || defined (__SSE2__)
#define zero_reg(regnum) "pxor %%xmm" regnum ", %%xmm" regnum "\n\t"
#endif

#define zero_simd()				\
  __asm__ __volatile__ (			\
    zero_reg("0")				\
    zero_reg("1")				\
    zero_reg("2")				\
    zero_reg("3")				\
    zero_reg("4")				\
    zero_reg("5")				\
    zero_reg("6")				\
    zero_reg("7")				\
    zero_reg("8")				\
    zero_reg("9")				\
    zero_reg("10")				\
    zero_reg("11")				\
    zero_reg("12")				\
    zero_reg("13")				\
    zero_reg("14")				\
    zero_reg("15")				\
    :::SIMD_CLOBBERED_REGS)


#if defined(_OPENMP)

#define asm_flops(in, out, type_str, op1, op2) do{			\
    volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;		\
    zero_simd();							\
    _Pragma("omp parallel")						\
    {									\
      _Pragma("omp barrier")						\
	_Pragma("omp master")						\
	roofline_rdtsc(c_high, c_low);					\
      __asm__ __volatile__ (						\
	"loop_flops_"type_str"_repeat:\n\t"				\
	simd_fp(op1,op2)						\
	"sub $1, %0\n\t"						\
	"jnz loop_flops_"type_str"_repeat\n\t"				\
	:: "r" (in->loop_repeat)	: SIMD_CLOBBERED_REGS);		\
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
       
#define asm_flops(in, out, type_str, op1, op2) do{			\
    volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;		\
    zero_simd();							\
    __asm__ __volatile__ (						\
      "CPUID\n\t"							\
      "RDTSC\n\t"							\
      "mov %%rdx, %0\n\t"						\
      "mov %%rax, %1\n\t"						\
      "loop_flops_"type_str"_repeat:\n\t"				\
      "sub $1, %4\n\t"							\
      simd_fp(op1,op2)							\
      "jnz loop_flops_"type_str"_repeat\n\t"				\
      "CPUID\n\t"							\
      "RDTSC\n\t"							\
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

#if defined (__AVX__)  || defined (__AVX2__)  ||defined (__AVX512__)
static void dprint_FUOP_by_ins(int fd, const char * op, int regmin, unsigned * regnum, int regmax){
  dprintf(fd, "\"%s %%%%%s%d, %%%%%s%d, %%%%%s%d\\n\\t\"\\\n",
	  op, SIMD_REG, *regnum, SIMD_REG, *regnum, SIMD_REG, *regnum);
  *regnum = regmin + ((*regnum)+1)%(regmax-regmin);
}
#else 
static void dprint_FUOP_by_ins(int fd, const char * op, unsigned * regnum){
  dprintf(fd, "\"%s %%%%%s%d, %%%%%s%d\\n\\t\"\\\n", op, SIMD_REG, *regnum, SIMD_REG, *regnum);
}
#endif


#define roofline_load_ins(stride,srcreg, regnum) SIMD_LOAD " "stride"("srcreg"),%%" SIMD_REG regnum"\n\t"
#define roofline_loadnt_ins(stride,srcreg, regnum) SIMD_LOAD_NT " "stride"("srcreg"),%%" SIMD_REG regnum"\n\t"
#define roofline_store_ins(stride,dstreg, regnum) SIMD_STORE " %%" SIMD_REG  regnum ", " stride "("dstreg")\n\t"
#define roofline_storent_ins(stride,dstreg, regnum) SIMD_STORE_NT " %%" SIMD_REG  regnum ", " stride "("dstreg")\n\t"

/*  unroll several instructions of above roofline_*_ins(parameter mem_uop) macro functions */
#if defined (__AVX512__)
#define SIMD_CHUNK_SIZE 1920

#define simd_mov(op1, op2, op3, op4, op5, op6,  datareg)	\
  op1("0",datareg,"0")						\
  op2("64",datareg,"1")						\
  op3("128",datareg,"2")					\
  op4("192",datareg,"3")					\
  op5("256",datareg,"4")					\
  op6("320",datareg,"5")					\
  op1("384",datareg,"6")					\
  op2("448",datareg,"7")					\
  op3("512",datareg,"8")					\
  op4("576",datareg,"9")					\
  op5("640",datareg,"10")					\
  op6("704",datareg,"11")					\
  op1("768",datareg,"12")					\
  op2("832",datareg,"13")					\
  op3("896",datareg,"14")					\
  op4("960",datareg,"15")					\
  op5("1024",datareg,"16")					\
  op6("1088",datareg,"17")					\
  op1("1152",datareg,"18")					\
  op2("1216",datareg,"19")					\
  op3("1280",datareg,"20")					\
  op4("1344",datareg,"21")					\
  op5("1408",datareg,"22")					\
  op6("1472",datareg,"23")					\
  op1("1536",datareg,"24")					\
  op2("1600",datareg,"25")					\
  op3("1664",datareg,"26")					\
  op4("1728",datareg,"27")					\
  op5("1792",datareg,"28")					\
  op6("1856",datareg,"29")
  
#elif defined (__AVX__)
#define SIMD_CHUNK_SIZE 1536
#define simd_mov(op1, op2, op3, op4, op5, op6,  datareg)	\
  op1("0",datareg,"0")						\
  op2("32",datareg,"1")						\
  op3("64",datareg,"2")						\
  op4("96",datareg,"3")						\
  op5("128",datareg,"4")					\
  op6("160",datareg,"5")					\
  op1("192",datareg,"6")					\
  op2("224",datareg,"7")					\
  op3("256",datareg,"8")					\
  op4("288",datareg,"9")					\
  op5("320",datareg,"10")					\
  op6("352",datareg,"11")					\
  op1("384",datareg,"12")					\
  op2("416",datareg,"13")					\
  op3("448",datareg,"14")					\
  op4("480",datareg,"15")					\
  op5("512",datareg,"0")					\
  op6("544",datareg,"1")					\
  op1("576",datareg,"2")					\
  op2("608",datareg,"3")					\
  op3("640",datareg,"4")					\
  op4("672",datareg,"5")					\
  op5("704",datareg,"6")					\
  op6("736",datareg,"7")					\
  op1("768",datareg,"8")					\
  op2("800",datareg,"9")					\
  op3("832",datareg,"10")					\
  op4("864",datareg,"11")					\
  op5("896",datareg,"12")					\
  op6("928",datareg,"13")					\
  op1("960",datareg,"14")					\
  op2("992",datareg,"15")					\
  op3("1024",datareg,"0")					\
  op4("1056",datareg,"1")					\
  op5("1088",datareg,"2")					\
  op6("1120",datareg,"3")					\
  op1("1152",datareg,"4")					\
  op2("1184",datareg,"5")					\
  op3("1216",datareg,"6")					\
  op4("1248",datareg,"7")					\
  op5("1280",datareg,"8")					\
  op6("1312",datareg,"9")					\
  op1("1344",datareg,"10")					\
  op2("1376",datareg,"11")					\
  op3("1408",datareg,"12")					\
  op4("1440",datareg,"13")					\
  op5("1472",datareg,"14")					\
  op6("1504",datareg,"15")
  
#elif (__SSE__) || defined (__SSE2__) || defined (__SSE4_1__)
#define SIMD_CHUNK_SIZE 768
#define _mov(op1, op2, op3, op4, op5, op6,  datareg)	\
  op1("0",datareg,"0")					\
  op2("16",datareg,"1")					\
  op3("32",datareg,"2")					\
  op4("48",datareg,"3")					\
  op5("64",datareg,"4")					\
  op6("80",datareg,"5")					\
  op1("96",datareg,"6")					\
  op2("112",datareg,"7")				\
  op3("128",datareg,"8")				\
  op4("144",datareg,"9")				\
  op5("160",datareg,"10")				\
  op6("176",datareg,"11")				\
  op1("192",datareg,"12")				\
  op2("208",datareg,"13")				\
  op3("224",datareg,"14")				\
  op4("240",datareg,"15")				\
  op5("256",datareg,"0")				\
  op6("272",datareg,"1")				\
  op1("288",datareg,"2")				\
  op2("304",datareg,"3")				\
  op3("320",datareg,"4")				\
  op4("336",datareg,"5")				\
  op5("352",datareg,"6")				\
  op6("368",datareg,"7")				\
  op1("384",datareg,"8")				\
  op2("400",datareg,"9")				\
  op3("416",datareg,"10")				\
  op4("432",datareg,"11")				\
  op5("448",datareg,"12")				\
  op6("464",datareg,"13")				\
  op1("480",datareg,"14")				\
  op2("496",datareg,"15")				\
  op3("512",datareg,"0")				\
  op4("528",datareg,"1")				\
  op5("544",datareg,"2")				\
  op6("560",datareg,"3")				\
  op1("576",datareg,"4")				\
  op2("592",datareg,"5")				\
  op3("608",datareg,"6")				\
  op4("624",datareg,"7")				\
  op5("640",datareg,"8")				\
  op6("656",datareg,"9")				\
  op1("672",datareg,"10")				\
  op2("688",datareg,"11")				\
  op3("704",datareg,"12")				\
  op4("720",datareg,"13")				\
  op5("736",datareg,"14")				\
  op6("752",datareg,"15")
#endif

#define asm_load				\
  roofline_load_ins,				\
    roofline_load_ins,				\
    roofline_load_ins,				\
    roofline_load_ins,				\
    roofline_load_ins,				\
    roofline_load_ins

#define asm_loadnt				\
  roofline_loadnt_ins,				\
    roofline_loadnt_ins,			\
    roofline_loadnt_ins,			\
    roofline_loadnt_ins,			\
    roofline_loadnt_ins,			\
    roofline_loadnt_ins
					
#define asm_store				\
  roofline_store_ins,				\
    roofline_store_ins,				\
    roofline_store_ins,				\
    roofline_store_ins,				\
    roofline_store_ins,				\
    roofline_store_ins
					
#define asm_storent				\
  roofline_storent_ins,				\
    roofline_storent_ins,			\
    roofline_storent_ins,			\
    roofline_storent_ins,			\
    roofline_storent_ins,			\
    roofline_storent_ins
					
#define asm_2ld1st				\
  roofline_load_ins,				\
    roofline_load_ins,				\
    roofline_store_ins,				\
    roofline_load_ins,				\
    roofline_load_ins,				\
    roofline_store_ins
					
#define asm_copy				\
  roofline_loadnt_ins,				\
    roofline_storent_ins,			\
    roofline_loadnt_ins,			\
    roofline_storent_ins,			\
    roofline_loadnt_ins,			\
    roofline_storent_ins

#define reg_mv "%%r11"

#ifdef _OPENMP
#define parallel_start _Pragma("omp parallel firstprivate(stream, size) proc_bind(close)")
#define parallel_end
#define rdtsc(c_high,c_low) _Pragma("omp barrier") _Pragma("omp master") roofline_rdtsc(c_high, c_low)
#define size_split(size) size /= omp_get_num_threads()
#define stream_pos(stream) stream = stream + omp_get_thread_num()*size/sizeof(*stream)
#define get_thread_num() omp_get_thread_num()
#else
#define parallel_start
#define parallel_end
#define rdtsc(c_high,c_low) roofline_rdtsc(c_high, c_low)
#define size_split(size) size = size
#define stream_pos(stream) stream = stream
#define get_thread_num() 0
#endif

#define asm_bandwidth(in, out, type_name, ...) do{			\
    uint64_t c_low0=0, c_low1=0, c_high0=0, c_high1=0;			\
    ROOFLINE_STREAM_TYPE * stream = in->stream;				\
    size_t size = in->stream_size;					\
    parallel_start{							\
      size_split(size);							\
      stream_pos(stream);						\
      zero_simd();							\
      rdtsc(c_high0, c_low0);						\
      __asm__ __volatile__ (						\
	"loop_"type_name"_repeat:\n\t"					\
	"mov %1, "reg_mv"\n\t"						\
	"mov %2, %%r12\n\t"						\
	"buffer_"type_name"_increment:\n\t"				\
	simd_mov(__VA_ARGS__, reg_mv)					\
	"add $"roofline_stringify(SIMD_CHUNK_SIZE)", "reg_mv"\n\t"	\
	"sub $"roofline_stringify(SIMD_CHUNK_SIZE)", %%r12\n\t"		\
	"jnz buffer_"type_name"_increment\n\t"				\
	"sub $1, %0\n\t"						\
	"jnz loop_"type_name"_repeat\n\t"				\
	:: "r" (in->loop_repeat), "r" (stream), "r" (size)		\
	: "%r11", "%r12", SIMD_CLOBBERED_REGS, "memory");		\
      rdtsc(c_high1, c_low1);						\
    }									\
    out->ts_start = roofline_rdtsc_diff(c_high0, c_low0);		\
    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);			\
    out->bytes = in->stream_size*in->loop_repeat;			\
    out->instructions = out->bytes/SIMD_BYTES;				\
  } while(0)

