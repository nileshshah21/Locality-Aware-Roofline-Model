#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "../roofline.h"
#include "MSC.h"

#define SIMD_CLOBBERED_REG "xmm"
#define SIMD_CLOBBERED_REGS "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13", "%xmm14", "%xmm15"

#if defined (__AVX512__)
#define SIMD_REG           "zmm"
#define SIMD_N_REGS        32
#define SIMD_BYTES         64
#define SIMD_FLOPS         8
#define SIMD_STORE         "vmovapd"
#define SIMD_LOAD          "vmovapd"
#define SIMD_MUL           "vmulpd"
#define SIMD_ADD           "vaddpd"
#elif defined (__AVX__)
#define SIMD_REG           "ymm"
#define SIMD_N_REGS        16
#define SIMD_BYTES         32
#define SIMD_FLOPS         4
#define SIMD_STORE         "vmovapd"
#define SIMD_LOAD          "vmovapd"
#define SIMD_MUL           "vmulpd"
#define SIMD_ADD           "vaddpd"
#elif defined (__SSE4_1__)
#define SIMD_REG           "xmm"
#define SIMD_N_REGS        16
#define SIMD_BYTES         16
#define SIMD_FLOPS         2
#define SIMD_STORE         "movapd"
#define SIMD_LOAD          "movapd"
#define SIMD_MUL           "mulpd"
#define SIMD_ADD           "addpd"
#elif defined (__SSE2__)
#define SIMD_REG           "xmm"
#define SIMD_N_REGS        16
#define SIMD_BYTES         16
#define SIMD_FLOPS         2
#define SIMD_STORE         "movapd"
#define SIMD_LOAD          "movapd"
#define SIMD_MUL           "mulpd"
#define SIMD_ADD           "addpd"
#elif defined (__SSE__)
#define SIMD_REG           "xmm"
#define SIMD_N_REGS        16
#define SIMD_BYTES         16
#define SIMD_FLOPS         4
#define SIMD_STORE         "movaps"
#define SIMD_LOAD          "movaps"
#define SIMD_MUL           "mulps"
#define SIMD_ADD           "addps"
#endif
#if defined (__FMA__)
#define SIMD_FMA           "vfmadd132pd"
#undef  SIMD_FLOPS
#define SIMD_FLOPS         8
#endif

#if defined (__AVX__)  || defined (__AVX512__) || defined (__FMA__)
#define simd_fp(op, a, b, c) op " %%" SIMD_REG a ", %%" SIMD_REG b ", %%" SIMD_REG c "\n\t"
#else 
#define simd_fp(op, a, b) op " %%" SIMD_REG a ", %%" SIMD_REG b "\n\t"
#endif
#define roofline_load_ins(stride,srcreg, regnum) SIMD_LOAD " "stride"("srcreg"),%%" SIMD_REG regnum"\n\t"
#define roofline_store_ins(stride,dstreg, regnum) SIMD_STORE " %%" SIMD_REG  regnum ", " stride "("dstreg")\n\t"

#if defined (__AVX512__)
#define SIMD_CHUNK_SIZE 2048
#define simd_mov(macro_bench, datareg)		\
    macro_bench("0",datareg,"0")		\
    macro_bench("64",datareg,"1")		\
    macro_bench("128",datareg,"2")		\
    macro_bench("192",datareg,"3")		\
    macro_bench("256",datareg,"4")		\
    macro_bench("320",datareg,"5")		\
    macro_bench("384",datareg,"6")		\
    macro_bench("448",datareg,"7")		\
    macro_bench("512",datareg,"8")		\
    macro_bench("576",datareg,"9")		\
    macro_bench("640",datareg,"10")		\
    macro_bench("704",datareg,"11")		\
    macro_bench("768",datareg,"12")		\
    macro_bench("832",datareg,"13")		\
    macro_bench("896",datareg,"14")		\
    macro_bench("960",datareg,"15")		\
    macro_bench("1024",datareg,"16")		\
    macro_bench("1088",datareg,"17")		\
    macro_bench("1152",datareg,"18")		\
    macro_bench("1216",datareg,"19")		\
    macro_bench("1280",datareg,"20")		\
    macro_bench("1344",datareg,"21")		\
    macro_bench("1408",datareg,"22")		\
    macro_bench("1472",datareg,"23")		\
    macro_bench("1536",datareg,"24")		\
    macro_bench("1600",datareg,"25")		\
    macro_bench("1664",datareg,"26")		\
    macro_bench("1728",datareg,"27")		\
    macro_bench("1792",datareg,"28")		\
    macro_bench("1856",datareg,"29")		\
    macro_bench("1920",datareg,"30")		\
    macro_bench("1984",datareg,"31")
#elif defined (__AVX__)
#define SIMD_CHUNK_SIZE 512
#define simd_mov(macro_bench, datareg)		\
    macro_bench("0",datareg,"0")		\
    macro_bench("32",datareg,"1")		\
    macro_bench("64",datareg,"2")		\
    macro_bench("96",datareg,"3")		\
    macro_bench("128",datareg,"4")		\
    macro_bench("160",datareg,"5")		\
    macro_bench("192",datareg,"6")		\
    macro_bench("224",datareg,"7")		\
    macro_bench("256",datareg,"8")		\
    macro_bench("288",datareg,"9")		\
    macro_bench("320",datareg,"10")		\
    macro_bench("352",datareg,"11")		\
    macro_bench("384",datareg,"12")		\
    macro_bench("416",datareg,"13")		\
    macro_bench("448",datareg,"14")		\
    macro_bench("480",datareg,"15")
#elif defined (__SSE__) || defined (__SSE2__) || defined (__SSE4_1__)
#define SIMD_CHUNK_SIZE 256
#define simd_mov(macro_bench, datareg)		\
    macro_bench("0",datareg,"0")		\
    macro_bench("16",datareg,"1")		\
    macro_bench("32",datareg,"2")		\
    macro_bench("48",datareg,"3")		\
    macro_bench("64",datareg,"4")		\
    macro_bench("80",datareg,"5")		\
    macro_bench("96",datareg,"6")		\
    macro_bench("112",datareg,"7")		\
    macro_bench("128",datareg,"8")		\
    macro_bench("144",datareg,"9")		\
    macro_bench("160",datareg,"10")		\
    macro_bench("176",datareg,"11")		\
    macro_bench("192",datareg,"12")		\
    macro_bench("208",datareg,"13")		\
    macro_bench("224",datareg,"14")		\
    macro_bench("240",datareg,"15")
#endif

size_t chunk_size = SIMD_CHUNK_SIZE;

#ifdef USE_OMP
#define bandwidth_bench_run(stream, size, repeat, id_str, macro_bench) \
	__asm__ __volatile__ (						\
			      "loop_"id_str"_repeat:\n\t"		\
			      "mov %1, %%r11\n\t"			\
			      "mov %2, %%r12\n\t"			\
			      "buffer_"id_str"_increment:\n\t"		\
			      simd_mov(macro_bench,"%%r11")		\
			      "add $"roofline_macro_xstr(SIMD_CHUNK_SIZE)", %%r11\n\t" \
			      "sub $"roofline_macro_xstr(SIMD_CHUNK_SIZE)", %%r12\n\t" \
			      "jnz buffer_"id_str"_increment\n\t"	\
			      "sub $1, %0\n\t"				\
			      "jnz loop_"id_str"_repeat\n\t"		\
			      :						\
			      : "r" (repeat), "r" (stream), "r" (size)	\
			      : "%r11", "%r12", SIMD_CLOBBERED_REGS, "memory")

extern int omp_get_thread_num(); 
extern int omp_get_num_threads(); 

void load_bandwidth_bench(struct roofline_sample_in * in, struct roofline_sample_out * out)
{
    volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
#pragma omp parallel
    {
	roofline_hwloc_cpubind();
	ROOFLINE_STREAM_TYPE * stream = NULL;
	unsigned long repeat = in->loop_repeat;
	size_t size = in->stream_size/n_threads;
	stream = in->stream + omp_get_thread_num()*size/sizeof(*stream);

#pragma omp barrier
#pragma omp master 
	roofline_rdtsc(c_high, c_low);

	bandwidth_bench_run(stream, size, repeat, "load", roofline_load_ins);

#pragma omp barrier
#pragma omp master 
	{
	    roofline_rdtsc(c_high1, c_low1);
	    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
	    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
	    out->bytes = n_threads*size*in->loop_repeat;
	    out->instructions = out->bytes/SIMD_BYTES;
	}
    }
}

void store_bandwidth_bench(struct roofline_sample_in * in, struct roofline_sample_out * out)
{
    volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
#pragma omp parallel
    {
	ROOFLINE_STREAM_TYPE * stream = NULL;
	unsigned long repeat = in->loop_repeat;
	size_t size = in->stream_size/n_threads; 
	roofline_hwloc_cpubind();
	stream = in->stream + omp_get_thread_num()*size/sizeof(*stream);
    
#pragma omp barrier
#pragma omp master 
	{
	    roofline_rdtsc(c_high, c_low);
	}
	bandwidth_bench_run(stream, size, repeat, "store", roofline_store_ins);

#pragma omp barrier
#pragma omp master 
	{
	    roofline_rdtsc(c_high1, c_low1);
	    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
	    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
	    out->bytes = n_threads*size*in->loop_repeat;
	    out->instructions = out->bytes/SIMD_BYTES;
	}
    }
}
#else
#define bandwidth_bench_run(c_low, c_high, c_low1, c_high1, stream, size, repeat, id_str, macro_bench) \
	__asm__ __volatile__ (						\
			      "CPUID\n\t"				\
			      "RDTSC\n\t"				\
			      "mov %%rdx, %0\n\t"			\
			      "mov %%rax, %1\n\t"			\
			      "loop_"id_str"_repeat:\n\t"		\
			      "mov %5, %%r11\n\t"			\
			      "mov %6, %%r12\n\t"			\
			      "buffer_"id_str"_increment:\n\t"		\
			      simd_mov(macro_bench,"%%r11")		\
			      "add $"roofline_macro_xstr(SIMD_CHUNK_SIZE)", %%r11\n\t" \
			      "sub $"roofline_macro_xstr(SIMD_CHUNK_SIZE)", %%r12\n\t" \
			      "jnz buffer_"id_str"_increment\n\t"	\
			      "sub $1, %4\n\t"				\
			      "jnz loop_"id_str"_repeat\n\t"		\
			      "CPUID\n\t"				\
			      "RDTSC\n\t"				\
			      "movq %%rdx, %2\n\t"			\
			      "movq %%rax, %3\n\t"			\
			      : "=&r" (c_high), "=&r" (c_low), "=&r" (c_high1), "=&r" (c_low1) \
			      : "r" (repeat), "r" (stream), "r" (size)	\
			      : "%rax", "%rbx", "%rcx", "%rdx", "%r11", "%r12", SIMD_CLOBBERED_REGS, "memory")

void load_bandwidth_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
    volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
    bandwidth_bench_run(c_low, c_high, c_low1, c_high1, in->stream, in->stream_size, in->loop_repeat, "load", roofline_load_ins);
    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
    out->instructions = in->stream_size*in->loop_repeat/SIMD_BYTES;
    out->bytes = out->instructions*SIMD_BYTES;
}

void store_bandwidth_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
    volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
    bandwidth_bench_run(c_low, c_high, c_low1, c_high1, in->stream, in->stream_size, in->loop_repeat, "store", roofline_store_ins);
    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
    out->instructions = in->stream_size*in->loop_repeat/SIMD_BYTES;
    out->bytes = out->instructions*SIMD_BYTES;
}
#endif


#if defined __AVX__ || defined __AVX512__
#define fpeak_instructions			\
    simd_fp(SIMD_ADD, "0", "1", "1")		\
    simd_fp(SIMD_MUL, "2", "3", "3")		\
    simd_fp(SIMD_ADD, "4", "5", "5")		\
    simd_fp(SIMD_MUL, "6", "7", "7")		\
    simd_fp(SIMD_ADD, "8", "9", "9")		\
    simd_fp(SIMD_MUL, "10", "11", "11")		\
    simd_fp(SIMD_ADD, "12", "13", "13")		\
    simd_fp(SIMD_MUL, "14", "15", "15")		\
    simd_fp(SIMD_ADD, "0", "1", "1")		\
    simd_fp(SIMD_MUL, "2", "3", "3")		\
    simd_fp(SIMD_ADD, "4", "5", "5")		\
    simd_fp(SIMD_MUL, "6", "7", "7")		\
    simd_fp(SIMD_ADD, "8", "9", "9")		\
    simd_fp(SIMD_MUL, "10", "11", "11")		\
    simd_fp(SIMD_ADD, "12", "13", "13")		\
    simd_fp(SIMD_MUL, "14", "15", "15")
#else
#define fpeak_instructions			\
    simd_fp(SIMD_ADD, "0", "1")			\
    simd_fp(SIMD_MUL, "2", "3")			\
    simd_fp(SIMD_ADD, "4", "5")			\
    simd_fp(SIMD_MUL, "6", "7")			\
    simd_fp(SIMD_ADD, "8", "9")			\
    simd_fp(SIMD_MUL, "10", "11")		\
    simd_fp(SIMD_ADD, "12", "13")		\
    simd_fp(SIMD_MUL, "14", "15")		\
    simd_fp(SIMD_ADD, "0", "1")			\
    simd_fp(SIMD_MUL, "2", "3")			\
    simd_fp(SIMD_ADD, "4", "5")			\
    simd_fp(SIMD_MUL, "6", "7")			\
    simd_fp(SIMD_ADD, "8", "9")			\
    simd_fp(SIMD_MUL, "10", "11")		\
    simd_fp(SIMD_ADD, "12", "13")		\
    simd_fp(SIMD_MUL, "14", "15")
#endif

#ifdef USE_OMP
void fpeak_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
    volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
     
#pragma omp parallel
    {
	roofline_hwloc_cpubind();

#pragma omp barrier
#pragma omp master 
	roofline_rdtsc(c_high, c_low);
	__asm__ __volatile__ (						\
			      "loop_flops_repeat:\n\t"			\
			      fpeak_instructions			\
			      "sub $1, %0\n\t"				\
			      "jnz loop_flops_repeat\n\t"		\
			      :: "r" (in->loop_repeat)			\
			      : SIMD_CLOBBERED_REGS);
#pragma omp barrier
#pragma omp master 
	{
	    roofline_rdtsc(c_high1, c_low1);
	    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
	    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
	    out->instructions = omp_get_num_threads()*16*in->loop_repeat;
	    out->flops = out->instructions * SIMD_FLOPS;
	}
    }
}
#else
void fpeak_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
    uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;
    long repeat = in->loop_repeat;
    roofline_hwloc_cpubind();
    __asm__ __volatile__ (						\
			  "CPUID\n\t"					\
			  "RDTSC\n\t"					\
			  "mov %%rdx, %0\n\t"				\
			  "mov %%rax, %1\n\t"				\
			  "loop_flops_repeat:\n\t"			\
			  fpeak_instructions				\
			  "sub $1, %4\n\t"				\
			  "jnz loop_flops_repeat\n\t"			\
			  "CPUID\n\t"					\
			  "RDTSC\n\t"					\
			  "movq %%rdx, %2\n\t"				\
			  "movq %%rax, %3\n\t"				\
			  : "=&r" (c_high), "=&r" (c_low), "=&r" (c_high1), "=&r" (c_low1) \
			  : "r" (repeat)				\
			  : "%rax", "%rbx", "%rcx", "%rdx", SIMD_CLOBBERED_REGS);
    out->ts_start = roofline_rdtsc_diff(c_high, c_low);
    out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);
    out->instructions = 16*in->loop_repeat;
    out->flops = out->instructions*SIMD_FLOPS;
}
#endif

/***************************************** OI BENCHMARKS GENERATION ******************************************/

#if defined (__AVX__)  || defined (__AVX512__)
static void dprint_FUOP(int fd, const char * op, unsigned * regnum){
    dprintf(fd, "\"%s %%%%%s%d, %%%%%s%d, %%%%%s%d\\n\\t\"\\\n",
	    op, SIMD_REG, *regnum, SIMD_REG, (*regnum+1)%SIMD_N_REGS, SIMD_REG, (*regnum+1)%SIMD_N_REGS);
    *regnum = (*regnum+2)%SIMD_N_REGS;
}
#else 
static void dprint_FUOP(int fd, const char * op, unsigned * regnum){
    dprintf(fd, "\"%s %%%%%s%d, %%%%%s%d\\n\\t\"\\\n",
	    op, SIMD_REG, *regnum, SIMD_REG, (*regnum+1)%SIMD_N_REGS);
    *regnum = (*regnum+2)%SIMD_N_REGS;
}
#endif


static void dprint_MUOP(int fd, int type, off_t * offset, unsigned * regnum, const char * datareg){
    if(type == ROOFLINE_LOAD)
	dprintf(fd, "\"%s %lu(%%%%%s), %%%%%s%d\\n\\t\"\\\n", SIMD_LOAD, *offset, datareg, SIMD_REG, *regnum);
    else if(type == ROOFLINE_STORE)
	dprintf(fd, "\"%s %%%%%s%d, %lu(%%%%%s)\\n\\t\"\\\n", SIMD_STORE, SIMD_REG, *regnum, *offset, datareg);
    *regnum = (*regnum+1)%SIMD_N_REGS;
    *offset+=SIMD_BYTES;
}

static void  dprint_header(int fd) {
    dprintf(fd, "#include <stdlib.h>\n");
#ifdef USE_OMP
    dprintf(fd, "#include <omp.h>\n");
#endif
    dprintf(fd, "#include \"roofline.h\"\n\n");
    dprintf(fd, "#define rdtsc(high,low) __asm__ __volatile__ (\"CPUID\\n\\tRDTSC\\n\\tmovq %%%%rdx, %%0\\n\\tmovq %%%%rax, %%1\\n\\t\" :\"=r\" (high), \"=r\" (low)::\"%%rax\", \"%%rbx\", \"%%rcx\", \"%%rdx\")\n\n");
}
 
static void dprint_oi_bench_begin(int fd, const char * id, const char * name){
    dprintf(fd, "void %s(struct roofline_sample_in * in, struct roofline_sample_out * out){\n", name);
    dprintf(fd, "volatile uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;\n");
    dprintf(fd, "ROOFLINE_STREAM_TYPE * stream = in->stream;\n");
    dprintf(fd, "size_t size = in->stream_size;\n");
#ifdef USE_OMP
    dprintf(fd,"#pragma omp parallel firstprivate(size, stream)\n{");
    dprintf(fd,"size /= omp_get_num_threads();\n");
    dprintf(fd,"stream += omp_get_thread_num()*size/sizeof(*stream);\n");
    dprintf(fd,"#pragma omp barrier\n");
    dprintf(fd,"#pragma omp master\n");
#endif
    dprintf(fd,"rdtsc(c_high,c_low);\n");
    dprintf(fd, "__asm__ __volatile__ (\\\n");
    dprintf(fd, "\"loop_%s_repeat:\\n\\t\"\\\n", id);
    dprintf(fd, "\"mov %%1, %%%%r10\\n\\t\"\\\n");
    dprintf(fd, "\"mov %%2, %%%%r11\\n\\t\"\\\n");
    dprintf(fd, "\"buffer_%s_increment:\\n\\t\"\\\n", id);
}

static void dprint_oi_bench_end(int fd, const char * id, off_t offset){
    dprintf(fd,"\"add $%lu, %%%%r10\\n\\t\"\\\n", offset);
    dprintf(fd,"\"sub $%lu, %%%%r11\\n\\t\"\\\n", offset);
    dprintf(fd,"\"jnz buffer_%s_increment\\n\\t\"\\\n", id);
    dprintf(fd,"\"sub $1, %%0\\n\\t\"\\\n");
    dprintf(fd,"\"jnz loop_%s_repeat\\n\\t\"\\\n", id);
    dprintf(fd,":: \"r\" (in->loop_repeat), \"r\" (stream), \"r\" (size)\\\n");
    dprintf(fd,": \"%%r10\", \"%%r11\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"memory\" );\n", SIMD_CLOBBERED_REGS);
#ifdef USE_OMP
    dprintf(fd,"#pragma omp barrier\n");
    dprintf(fd,"#pragma omp master\n");
#endif
    dprintf(fd,"rdtsc(c_high1,c_low1);\n");
#ifdef USE_OMP
    dprintf(fd,"}\n");
#endif
    dprintf(fd, "out->ts_end = ((c_high1 << 32) | c_low1);\n");
    dprintf(fd, "out->ts_start = ((c_high << 32) | c_low);\n");
}


static char * roofline_mkstemp()
{ 
    char tempname[1024], cwd[1024];
    memset(tempname,0,sizeof(tempname));
    memset(cwd,0,sizeof(cwd));

    if(getcwd(cwd, 1024) == NULL){
    	sprintf(cwd, "/tmp");
    }
    snprintf(tempname,1024,"%s/roofline_benchmark_XXXXXX",cwd);
    if(!strcmp(mktemp(tempname),"")){
	errEXIT("Cannot create temporary name");
    }
    return strdup(tempname);
}

extern char * compiler;

static int roofline_compile_lib(char * c_path, char* so_path){
    char cmd[2048];
    memset(cmd,0,sizeof(cmd));
#ifdef USE_OMP
    sprintf(cmd, "%s -fPIC -shared %s -rdynamic -o %s -fopenmp", compiler, c_path, so_path);
#else
    sprintf(cmd, "%s -fPIC -shared %s -rdynamic -o %s", compiler, c_path, so_path);
#endif
    return system(cmd);
}

static void (* roofline_load_lib(char * lib_path, char * funname)) (struct roofline_sample_in * in, struct roofline_sample_out * out)
{
    void * handle, * benchmark;

    handle = dlopen(lib_path, RTLD_NOW);
    if(handle == NULL){
	fprintf(stderr,"Failed to open dynamic library: %s\n%s\n",lib_path,dlerror());
	exit(EXIT_FAILURE);
    }
    dlerror();
    benchmark = dlsym(handle, funname);
    if(benchmark == NULL){
	fprintf(stderr, "Failed to load function %s from dynamic library %s\n", funname, lib_path);
    }
    return (void (*)(struct roofline_sample_in *, struct roofline_sample_out *))benchmark;
}


static off_t roofline_benchmark_write_oi_bench(int fd, const char * name, int type, double oi){
    off_t offset;
    unsigned i, regnum, mem_instructions, fop_instructions, fop_per_mop, mop_per_fop;

    if(oi<=0)
	return 0;
    regnum=0;
    offset = 0;         /* the offset of each load instruction */
    mem_instructions=0; /* The number of printed memory instructions */
    fop_instructions=0; /* The number of printed flop instructions */
    fop_per_mop = oi * SIMD_BYTES / SIMD_FLOPS;
    mop_per_fop = SIMD_FLOPS / (oi * SIMD_BYTES);
    dprint_oi_bench_begin(fd, roofline_type_str(type), name);
    
    if(mop_per_fop == 1){
	for(i=0;i<SIMD_N_REGS;i++){
	    dprint_MUOP(fd, type, &offset, &regnum, "r10");
	    if(i%2==0){dprint_FUOP(fd, SIMD_MUL, &regnum);}
	    if(i%2==1){dprint_FUOP(fd, SIMD_ADD, &regnum);}
	}
	mem_instructions = fop_instructions = SIMD_N_REGS;
    }
    else if(mop_per_fop > 1){
	mem_instructions = roofline_PPCM(mop_per_fop, SIMD_N_REGS);
	fop_instructions = mem_instructions / mop_per_fop;
	for(i=0;i<mem_instructions;i++){
	    dprint_MUOP(fd, type, &offset, &regnum, "r10");
	    if(i%mop_per_fop==0){
		fop_instructions++;
		if(i%2==0){dprint_FUOP(fd, SIMD_MUL, &regnum);}
		if(i%2==1){dprint_FUOP(fd, SIMD_ADD, &regnum);}
	    }
	}
    }
    else if(fop_per_mop > 1){
        fop_instructions = roofline_PPCM(fop_per_mop, SIMD_N_REGS);
	mem_instructions = fop_instructions / fop_per_mop;
	for(i=0;i<fop_instructions;i++){
	    if(i%fop_per_mop == 0) {
		dprint_MUOP(fd, type, &offset, &regnum, "r10");
	    }
	    if(i%2==0){dprint_FUOP(fd, SIMD_MUL, &regnum);}
	    if(i%2==1){dprint_FUOP(fd, SIMD_ADD, &regnum);}
	}
    }
    dprint_oi_bench_end(fd, roofline_type_str(type), offset);
    dprintf(fd, "out->instructions = in->loop_repeat * %u * in->stream_size / %lu;\n", mem_instructions+fop_instructions, offset);
    dprintf(fd, "out->bytes = in->loop_repeat * in->stream_size;\n");
    dprintf(fd, "out->flops = in->loop_repeat * %u * in->stream_size / %lu;\n", fop_instructions*SIMD_FLOPS, offset);
    dprintf(fd, "}\n\n");
    return offset;
}



void (* roofline_oi_bench(double oi, int type))(struct roofline_sample_in * in, struct roofline_sample_out * out)
{
    char * tempname, * benchname, * c_path, * so_path;
    size_t len;
    int fd; 
    void (* benchmark) (struct roofline_sample_in * in, struct roofline_sample_out * out);

    /* If oi is 0, then its a bandwidth benchmark, there is no need to generate it.*/
    if(oi==0){
	chunk_size = SIMD_CHUNK_SIZE;
	switch(type){
	case ROOFLINE_LOAD:
	    return load_bandwidth_bench;
	    break;
	case ROOFLINE_STORE:
	    return store_bandwidth_bench;
	    break;
	default:
	    return NULL;
	    break;
	}
    }

    /* Else we create one */
    /* Create the filename */
    tempname = roofline_mkstemp();
    len = strlen(tempname)+3; c_path =  malloc(len); memset(c_path,  0, len); snprintf(c_path, len, "%s.c", tempname);
    len = strlen(tempname)+4; so_path = malloc(len); memset(so_path, 0, len); snprintf(so_path, len, "%s.so", tempname);
    free(tempname);
    fd = open(c_path, O_WRONLY|O_CREAT, 0644);
    
    /* Write the appropriate roofline to the file */
    dprint_header(fd);

    benchname = "benchmark_roofline";
    chunk_size = roofline_benchmark_write_oi_bench(fd, benchname, type, oi);
    
    /* Compile the roofline function */
    close(fd); 
    roofline_compile_lib(c_path, so_path);
    unlink(c_path);free(c_path);
    /* Load the roofline function */
    benchmark = roofline_load_lib(so_path, benchname);
    unlink(so_path);free(so_path);
    return benchmark;
}

