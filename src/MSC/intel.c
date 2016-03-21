#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "../roofline.h"
#include "MSC.h"
#ifdef USE_OMP
#include <omp.h>
#endif

#define SIMD_CLOBBERED_REG "xmm"
#define SIMD_CLOBBERED_REGS "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7", "%xmm8", "%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13", "%xmm14", "%xmm15"

#ifdef USE_AVX512
#define SIMD_REG           "zmm"
#define SIMD_N_REGS        32
#define SIMD_REG_SIZE      64
#define SIMD_REG_DOUBLE    8
#elif defined USE_AVX
#define SIMD_REG           "ymm"
#define SIMD_N_REGS        16
#define SIMD_REG_SIZE      32
#define SIMD_REG_DOUBLE    4
#else
#define SIMD_REG           "xmm"
#define SIMD_N_REGS        16
#define SIMD_REG_SIZE      16
#define SIMD_REG_DOUBLE    2
#endif

#if defined USE_AVX512 || defined USE_AVX
#define SIMD_MOV           "vmovapd"
#define SIMD_MUL           "vmulpd"
#define SIMD_ADD           "vaddpd"
#elif defined USE_SSE || defined USE_SSE2
#define SIMD_MUL           "mulpd"
#define SIMD_ADD           "addpd"
#define SIMD_MOV           "movapd"
#else
#define SIMD_MUL           "mulsd"
#define SIMD_ADD           "addsd"
#define SIMD_MOV           "movsd"
#endif

#define roofline_load_ins(stride,srcreg, regnum) SIMD_MOV " "stride"("srcreg"),%%" SIMD_REG regnum"\n\t"
#define roofline_store_ins(stride,dstreg, regnum) SIMD_MOV " %%" SIMD_REG  regnum ", " stride "("dstreg")\n\t"

#ifdef USE_AVX512
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
#elif defined USE_AVX
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
#elif defined USE_SSE || defined USE_SSE2    
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
#else
#define SIMD_CHUNK_SIZE 128
#define simd_mov(macro_bench, datareg)		\
    macro_bench("0",datareg,"0")		\
    macro_bench("8",datareg,"1")		\
    macro_bench("16",datareg,"2")		\
    macro_bench("24",datareg,"3")		\
    macro_bench("32",datareg,"4")		\
    macro_bench("40",datareg,"5")		\
    macro_bench("48",datareg,"6")		\
    macro_bench("56",datareg,"7")		\
    macro_bench("64",datareg,"8")		\
    macro_bench("72",datareg,"9")		\
    macro_bench("80",datareg,"10")		\
    macro_bench("88",datareg,"11")		\
    macro_bench("96",datareg,"12")		\
    macro_bench("104",datareg,"13")		\
    macro_bench("112",datareg,"14")		\
    macro_bench("120",datareg,"15")
#endif /* USE_AVX */

static size_t chunk_size = SIMD_CHUNK_SIZE;
size_t alloc_chunk_aligned(double ** data, size_t size){
    int err;
    size += (chunk_size - size%chunk_size);
    if(data != NULL){
	err = posix_memalign((void**)data, alignement, size);
	switch(err){
	case 0:
	    break;
	case EINVAL:
	    fprintf(stderr,"The alignment argument was not a power of two, or was not a multiple of sizeof(void *).\n");
	    break;
	case ENOMEM:
	    fprintf(stderr,"There was insufficient memory to fulfill the allocation request.\n");
	}
	if(*data == NULL)
	    fprintf(stderr,"Chunk is NULL\n");
	if(err || *data == NULL)
	    errEXIT("");

    	memset(*data,0,size);
    }
    return size;
}

#define bandwidth_bench_run(in, out, id_str, macro_bench) do{ 		\
	uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;		\
	__asm__ __volatile__ (						\
			      "mov %4, %%r8\n\t"			\
			      "mov %5, %%r9\n\t"			\
			      "mov %6, %%r10\n\t"			\
			      "CPUID\n\t"				\
			      "RDTSC\n\t"				\
			      "mov %%rdx, %0\n\t"			\
			      "mov %%rax, %1\n\t"			\
			      "loop_"id_str"_repeat:\n\t"		\
			      "mov %%r9, %%r11\n\t"			\
			      "mov %%r10, %%r12\n\t"			\
			      "buffer_"id_str"_increment:\n\t"		\
			      simd_mov(macro_bench,"%%r11")	\
			      "add $"roofline_macro_xstr(SIMD_CHUNK_SIZE)", %%r11\n\t" \
			      "sub $"roofline_macro_xstr(SIMD_CHUNK_SIZE)", %%r12\n\t" \
			      "jnz buffer_"id_str"_increment\n\t"	\
			      "sub $1, %%r8\n\t"			\
			      "jnz loop_"id_str"_repeat\n\t"		\
			      "CPUID\n\t"				\
			      "RDTSC\n\t"				\
			      "movq %%rdx, %2\n\t"			\
			      "movq %%rax, %3\n\t"			\
			      : "=r" (c_high), "=r" (c_low), "=r" (c_high1), "=r" (c_low1) \
			      : "g" (in->loop_repeat), "o" (in->stream), "g" (in->stream_size) \
			      : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11", "%r12", SIMD_CLOBBERED_REGS, "memory"); \
	out->ts_start = roofline_rdtsc_diff(c_high, c_low);		\
	out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);		\
	out->instructions = in->stream_size*in->loop_repeat/SIMD_REG_SIZE; \
	out->bytes = out->instructions*SIMD_REG_SIZE;			\
	out->flops = 0;							\
    } while(0)


void load_bandwidth_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
#ifdef USE_OMP
    struct roofline_sample_out t_out;
    t_out.ts_start = 0;
    t_out.ts_end = 0;
    t_out.bytes = 0;
    t_out.flops = 0;
    t_out.instructions = 0;
#pragma omp parallel
    {
	size_t size;
	off_t offset;
	struct roofline_sample_in t_in;

	size = in->stream_size/omp_get_num_threads();
	size-=size%chunk_size;
	offset = size * omp_get_thread_num() / sizeof(*(in->stream));

	t_in.loop_repeat = in->loop_repeat;
	t_in.stream = in->stream+offset;
	t_in.stream_size = size;
	bandwidth_bench_run((&t_in), (&t_out), "load", roofline_load_ins);
#pragma omp critical
	roofline_output_accumulate(out,&t_out);
    }
#else
    bandwidth_bench_run(in, out, "load", roofline_load_ins);
#endif
}

void store_bandwidth_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
#ifdef USE_OMP
    struct roofline_sample_out t_out;
    t_out.ts_start = 0;
    t_out.ts_end = 0;
    t_out.bytes = 0;
    t_out.flops = 0;
    t_out.instructions = 0;
#pragma omp parallel
    {
	size_t size;
	off_t offset;
	struct roofline_sample_in t_in;

	size = in->stream_size/omp_get_num_threads();
	size-=size%chunk_size;
	offset = size * omp_get_thread_num() / sizeof(double);

	t_in.loop_repeat = in->loop_repeat;
	t_in.stream = in->stream+offset;
	t_in.stream_size = size;
	bandwidth_bench_run((&t_in), (&t_out), "store", roofline_store_ins);
#pragma omp critical
	roofline_output_accumulate(out,&t_out);
    }
#else
    bandwidth_bench_run(in, out, "store", roofline_store_ins);
#endif
}


#if defined USE_AVX  || defined USE_AVX512
#define simd_fp(op, regnum) op " %%" SIMD_REG regnum ", %%" SIMD_REG regnum ", %%" SIMD_REG regnum "\n\t"
#else 
#define simd_fp(op, regnum) op " %%" SIMD_REG regnum ", %%" SIMD_REG regnum "\n\t"
#endif

#define fpeak_bench_run(in,out) do{						\
	uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;		\
	__asm__ __volatile__ (						\
			      "mov %4, %%r8\n\t"			\
			      "CPUID\n\t"				\
			      "RDTSC\n\t"				\
			      "mov %%rdx, %0\n\t"			\
			      "mov %%rax, %1\n\t"			\
			      "loop_flops_repeat:\n\t"			\
			      simd_fp(SIMD_ADD, "0")			\
			      simd_fp(SIMD_MUL, "1")			\
			      simd_fp(SIMD_ADD, "2")			\
			      simd_fp(SIMD_MUL, "3")			\
			      simd_fp(SIMD_ADD, "4")			\
			      simd_fp(SIMD_MUL, "5")			\
			      simd_fp(SIMD_ADD, "6")			\
			      simd_fp(SIMD_MUL, "7")			\
			      simd_fp(SIMD_ADD, "8")			\
			      simd_fp(SIMD_MUL, "9")			\
			      simd_fp(SIMD_ADD, "10")			\
			      simd_fp(SIMD_MUL, "11")			\
			      simd_fp(SIMD_ADD, "12")			\
			      simd_fp(SIMD_MUL, "13")			\
			      simd_fp(SIMD_ADD, "14")			\
			      simd_fp(SIMD_MUL, "15")			\
			      "sub $1, %%r8\n\t"			\
			      "jnz loop_flops_repeat\n\t"		\
			      "CPUID\n\t"				\
			      "RDTSC\n\t"				\
			      "movq %%rdx, %2\n\t"			\
			      "movq %%rax, %3\n\t"			\
			      : "=r" (c_high), "=r" (c_low), "=r" (c_high1), "=r" (c_low1) \
			      : "r" (in->loop_repeat)			\
			      : "%rax", "%rbx", "%rcx", "%r8", SIMD_CLOBBERED_REGS, "memory"); \
	out->ts_start = roofline_rdtsc_diff(c_high, c_low);		\
	out->ts_end = roofline_rdtsc_diff(c_high1, c_low1);		\
	out->instructions = 16*in->loop_repeat;				\
	out->flops = out->instructions*SIMD_REG_DOUBLE;			\
    } while(0)


void fpeak_bench(struct roofline_sample_in * in, struct roofline_sample_out * out){
#ifdef USE_OMP
    struct roofline_sample_out t_out;
    t_out.ts_start = 0;
    t_out.ts_end = 0;
    t_out.bytes = 0;
    t_out.flops = 0;
    t_out.instructions = 0;
#pragma omp parallel
    {
	fpeak_bench_run(in,(&t_out));
#pragma omp critical
	roofline_output_accumulate(out,&t_out);
    }
#else
    fpeak_bench_run(in,out);
#endif
}

/***************************************** OI BENCHMARKS GENERATION ******************************************/

static void dprint_FUOP(int fd, const char * op, unsigned * regnum){
    dprintf(fd, "\"%s %%%%%s%d, %%%%%s%d, %%%%%s%d\\n\\t\"\\\n",
	    op, SIMD_REG, *regnum, SIMD_REG, *regnum, SIMD_REG, *regnum);
	*regnum = (*regnum+1)%SIMD_N_REGS;
}

static void dprint_MUOP(int fd, int type, off_t * offset, unsigned * simdregnum, const char * datareg){
    if(type == ROOFLINE_LOAD)
	dprintf(fd, "\"%s %lu(%%%%%s), %%%%%s%d\\n\\t\"\\\n", SIMD_MOV, *offset, datareg, SIMD_REG, *simdregnum);
    else if(type == ROOFLINE_STORE)
	dprintf(fd, "\"%s %%%%%s%d, %lu(%%%%%s)\\n\\t\"\\\n", SIMD_MOV, SIMD_REG, *simdregnum, *offset, datareg);
    *simdregnum = (*simdregnum+1)%SIMD_N_REGS;
    *offset+=SIMD_REG_SIZE;
}

static void  dprint_header(int fd) {
    dprintf(fd, "#include <stdlib.h>\n");
    dprintf(fd, "#include \"roofline.h\"\n\n");
}
 
static void dprint_oi_bench_begin(int fd, const char * id, const char * name){
    dprintf(fd, "void %s(struct roofline_sample_in * in, struct roofline_sample_out * out){\n", name);
    dprintf(fd, "uint64_t c_low=0, c_low1=0, c_high=0, c_high1=0;\n");
    dprintf(fd, "__asm__ __volatile__ (    \\\n");
    dprintf(fd, "\"mov %%4, %%%%r8     \\n\\t\"\\\n");
    dprintf(fd, "\"mov %%5, %%%%r9     \\n\\t\"\\\n");
    dprintf(fd, "\"mov %%6, %%%%r10    \\n\\t\"\\\n");
    dprintf(fd, "\"CPUID               \\n\\t\"\\\n");
    dprintf(fd, "\"RDTSC               \\n\\t\"\\\n");
    dprintf(fd, "\"mov %%%%rdx, %%0    \\n\\t\"\\\n");
    dprintf(fd, "\"mov %%%%rax, %%1    \\n\\t\"\\\n");
    dprintf(fd, "\"loop_%s_repeat:     \\n\\t\"\\\n", id);
    dprintf(fd, "\"mov %%%%r9,  %%%%r11\\n\\t\"\\\n");
    dprintf(fd, "\"mov %%%%r10, %%%%r12\\n\\t\"\\\n");
    dprintf(fd, "\"buffer_%s_increment:\\n\\t\"\\\n", id);
}

static void dprint_oi_bench_end(int fd, const char * id, off_t offset){
    dprintf(fd,"\"add $%lu, %%%%r11      \\n\\t\"\\\n", offset);
    dprintf(fd,"\"sub $%lu, %%%%r12      \\n\\t\"\\\n", offset);
    dprintf(fd,"\"jnz buffer_%s_increment\\n\\t\"\\\n", id);
    dprintf(fd,"\"sub $1, %%%%r8         \\n\\t\"\\\n");
    dprintf(fd,"\"jnz loop_%s_repeat     \\n\\t\"\\\n", id);
    dprintf(fd,"\"CPUID                  \\n\\t\"\\\n");
    dprintf(fd,"\"RDTSC                  \\n\\t\"\\\n");
    dprintf(fd,"\"movq %%%%rdx, %%2      \\n\\t\"\\\n");
    dprintf(fd,"\"movq %%%%rax, %%3      \\n\\t\"\\\n");
    dprintf(fd,": \"=r\" (c_high), \"=r\" (c_low), \"=r\" (c_high1), \"=r\" (c_low1)  \\\n");
    dprintf(fd,": \"g\" (in->loop_repeat), \"o\" (in->stream), \"g\" (in->stream_size)\\\n");
    dprintf(fd,": \"%%rax\", \"%%rbx\", \"%%rcx\", \"%%rdx\", \"%%r8\", \"%%r9\", \"%%r10\", \"%%r11\", \"%%r12\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"memory\" );\n", SIMD_CLOBBERED_REGS);
    dprintf(fd, "out->ts_start = ((c_high << 32) | c_low);\n");
    dprintf(fd, "out->ts_end = ((c_high1 << 32) | c_low1);\n");
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
    sprintf(cmd, "%s -g -O0 -fPIC -shared %s -rdynamic -o %s", compiler, c_path, so_path);
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


static unsigned roofline_PGCD(unsigned a, unsigned b){
    unsigned max,min, r;
    max = roofline_MAX(a,b);
    min = roofline_MIN(a,b);
    while((r=max%min)!=0){max = min; min = r;}
    return min;
}

static unsigned roofline_PPCM(unsigned a, unsigned b){
    return a*b/roofline_PGCD(a,b);
}

static off_t roofline_benchmark_write_oi_bench(int fd, const char * name, int type, double oi){
    off_t offset;
    unsigned i, regnum, mem_instructions, fop_instructions, fop_per_mop, mop_per_fop, n_loop;

    if(oi<=0)
	return 0;
    regnum=0;
    offset = 0;         /* the offset of each load instruction */
    mem_instructions=0; /* The number of printed memory instructions */
    fop_instructions=0; /* The number of printed flop instructions */
    fop_per_mop = oi * SIMD_REG_SIZE / SIMD_REG_DOUBLE;
    mop_per_fop = SIMD_REG_DOUBLE / (oi * SIMD_REG_SIZE);
    dprint_oi_bench_begin(fd, roofline_type_str(type), name);
    
    if(mop_per_fop == 1){
	for(i=0;i<SIMD_N_REGS;i++){
	    dprint_MUOP(fd, type, &offset, &regnum, "r11");
	    if(i%2==0){dprint_FUOP(fd, SIMD_MUL, &regnum);}
	    if(i%2==1){dprint_FUOP(fd, SIMD_ADD, &regnum);}
	}
	mem_instructions = fop_instructions = SIMD_N_REGS;
    }
    else if(mop_per_fop > 1){
	n_loop = roofline_PPCM(mop_per_fop, SIMD_N_REGS);
	for(i=0;i<n_loop;i++){
	    dprint_MUOP(fd, type, &offset, &regnum, "r11");
	    if(i%mop_per_fop==0){
		if(i%2==0){dprint_FUOP(fd, SIMD_MUL, &regnum); }
		if(i%2==1){dprint_FUOP(fd, SIMD_ADD, &regnum); }
	    }
	}
	mem_instructions = n_loop;
	fop_instructions = n_loop/mop_per_fop;
    }
    else if(fop_per_mop > 1){
	n_loop = roofline_PPCM(fop_per_mop, SIMD_N_REGS);
	for(i=0;i<n_loop;i++){
	    if(i%fop_per_mop == 0) {dprint_MUOP(fd, type, &offset, &regnum, "r11");}
	    if(i%2==0) {dprint_FUOP(fd, SIMD_MUL, &regnum); }
	    if(i%2==1) {dprint_FUOP(fd, SIMD_ADD, &regnum); }
	}
	mem_instructions = n_loop/fop_per_mop;
	fop_instructions = n_loop;
    }

    dprint_oi_bench_end(fd, roofline_type_str(type), offset);
    dprintf(fd, "out->instructions = in->loop_repeat * %u * in->stream_size / %lu;\n", mem_instructions+fop_instructions, offset);
    dprintf(fd, "out->bytes = in->loop_repeat * in->stream_size;\n");
    dprintf(fd, "out->flops = in->loop_repeat * %u * in->stream_size / %lu;\n", fop_instructions*SIMD_REG_DOUBLE, offset);
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
#ifdef USE_OMP
    chunk_size = roofline_benchmark_write_oi_bench(fd, benchname, type, oi)*omp_get_max_threads();
#else
    chunk_size = roofline_benchmark_write_oi_bench(fd, benchname, type, oi);
#endif
    
    /* Compile the roofline function */
    close(fd); 
    roofline_compile_lib(c_path, so_path);
    unlink(c_path); free(c_path);
    /* Load the roofline function */
    benchmark = roofline_load_lib(so_path, benchname);
    unlink(so_path); free(so_path);
    return benchmark;
}

