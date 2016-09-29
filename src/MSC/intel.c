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
#include "macros.h"

static size_t oi_chunk_size = SIMD_CHUNK_SIZE;

size_t get_chunk_size(int type){
  int mem_type = type & (ROOFLINE_LOAD|ROOFLINE_LOAD_NT|ROOFLINE_STORE|ROOFLINE_STORE_NT|ROOFLINE_2LD1ST|ROOFLINE_COPY);
  int flop_type = type & (ROOFLINE_ADD|ROOFLINE_MUL|ROOFLINE_MAD|ROOFLINE_FMA);
  if(flop_type && mem_type) return oi_chunk_size;
  else if(mem_type) return SIMD_CHUNK_SIZE;
  else return 0;
}

/******************************** Flops loop instructions ***************************/


void fpeak_benchmark(const struct roofline_sample_in * in, struct roofline_sample_out * out, int type){
  switch(type){
  case ROOFLINE_ADD:
    asm_flops(in, out, "add", SIMD_ADD, SIMD_ADD);
    break;
  case ROOFLINE_MUL:
    asm_flops(in, out, "mul", SIMD_MUL, SIMD_MUL);
    break;
  case ROOFLINE_MAD:
    asm_flops(in, out, "mad", SIMD_MUL, SIMD_ADD);
    break;
#if defined (__FMA__)
  case ROOFLINE_FMA:
    asm_flops(in, out, "fma", SIMD_FMA, SIMD_FMA);
    out->flops*=2;
    break;
#endif
  default:
    break;
  }
}

/******************************** Memory loop instructions ***************************/

void bandwidth_benchmark(const struct roofline_sample_in * in, struct roofline_sample_out * out, int type){
  switch(type){
  case ROOFLINE_LOAD:
    asm_bandwidth(in, out, "load", asm_load);
    break;
  case ROOFLINE_LOAD_NT:
    asm_bandwidth(in, out, "loadnt", asm_loadnt);
    break;
  case ROOFLINE_STORE:
    asm_bandwidth(in, out, "store", asm_store);
    break;
  case ROOFLINE_STORE_NT:
    asm_bandwidth(in, out, "storent", asm_storent);
    break;
  case ROOFLINE_2LD1ST:
    asm_bandwidth(in, out, "2ld1st", asm_2ld1st);
    break;
  case ROOFLINE_COPY:
    asm_bandwidth(in, out, "copy", asm_copy);
    break;
  default:
    break;
  }
}

/***************************************** OI BENCHMARKS GENERATION ******************************************/

static void dprint_FUOP(int fd, int type, int i, unsigned regmin, unsigned * regnum, unsigned regmax){
  switch(type){
  case ROOFLINE_ADD:
    dprint_FUOP_by_ins(fd, SIMD_ADD, regnum);
    break;
  case ROOFLINE_MUL:
    dprint_FUOP_by_ins(fd, SIMD_MUL, regnum);
    break;
  case ROOFLINE_MAD:
    if(i%2) return dprint_FUOP_by_ins(fd, SIMD_MUL, regnum);
    else return dprint_FUOP_by_ins(fd, SIMD_ADD, regnum);
    break;
#if defined (__FMA__)  && defined (__AVX__)
  case ROOFLINE_FMA:
    dprint_FUOP_by_ins(fd, SIMD_FMA, regnum);
    break;
#endif
  default:
    break;
  }
  *regnum = regmin + (*regnum+1)%(regmax-regmin);
}


static void dprint_MUOP(int fd, int type, int i, off_t * offset, const unsigned regmin, unsigned * regnum, const unsigned regmax, const char * datareg){
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
    if(i%3) return dprint_MUOP(fd,ROOFLINE_LOAD,i,offset,regmin,regnum,regmax,datareg);
    else return dprint_MUOP(fd,ROOFLINE_STORE,i,offset,regmin, regnum, regmax, datareg);
    break;
  case ROOFLINE_COPY:
    if(i%2) return dprint_MUOP(fd,ROOFLINE_LOAD,i,offset,regmin,regnum,regmax,datareg);
    else return dprint_MUOP(fd,ROOFLINE_STORE,i,offset,regmin,regnum,regmax,datareg);
    break;
  default:
    break;
  }
  *offset+=SIMD_BYTES;
  *regnum = regmin + (*regnum+1)%(regmax-regmin);
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
 
static void dprint_oi_bench_begin(int fd, const char * id, const char * name, int type){
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
  if(type & ROOFLINE_FMA){
    dprintf(fd, "\"vmovapd (%%1),    %%%%ymm0\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm1\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm2\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm3\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm4\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm5\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm6\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm7\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm8\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm9\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm10\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm11\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm12\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm13\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm14\\n\\t\"\n");
    dprintf(fd, "\"vmovapd %%%%ymm0, %%%%ymm15\\n\\t\"\n");
  }
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
  sprintf(cmd, "%s -g -fPIC -shared %s -rdynamic -o %s %s", compiler, c_path, so_path, omp_flag);
  return system(cmd);
}

off_t roofline_benchmark_write_oi_bench(int fd, const char * name, int mem_type, int flop_type, double oi){
  off_t offset;
  size_t len;
  unsigned i, mem_instructions = 0, fop_instructions = 0, fop_per_mop, mop_per_fop;
  /* min, val, max */
  unsigned flop_regs[3], mem_regs[3], *flop_regnum, * mem_regnum;
  char * idx;
  if(oi<=0)return 0;
    
  len = 2+strlen(roofline_type_str(mem_type))+strlen(roofline_type_str(flop_type));
  idx = malloc(len);
  memset(idx,0,len);
  snprintf(idx,len,"%s_%s", roofline_type_str(mem_type), roofline_type_str(flop_type));
  offset = 0;         /* the offset of each load/store instruction */
  fop_per_mop = oi * SIMD_BYTES / SIMD_FLOPS;
  mop_per_fop = SIMD_FLOPS / (oi * SIMD_BYTES);

  fop_per_mop = oi * SIMD_BYTES / SIMD_FLOPS;
  mop_per_fop = SIMD_FLOPS / (oi * SIMD_BYTES);    
    
  if(flop_type == ROOFLINE_FMA){
    fop_per_mop = oi * SIMD_BYTES / (2*SIMD_FLOPS);
    mop_per_fop = SIMD_FLOPS*2 / (oi * SIMD_BYTES);
  }

  mem_regs[0]  = 0; mem_regs[1]  = 0; mem_regs[2]  = SIMD_N_REGS;
  flop_regs[0] = 0; flop_regs[1] = 0; flop_regs[2] = SIMD_N_REGS;
  flop_regnum = &(mem_regs[1]); mem_regnum = &(mem_regs[1]);
  
  dprint_oi_bench_begin(fd, idx, name, flop_type);
  if(mop_per_fop == 1){
    unsigned n_ins = SIMD_N_REGS*6;
    mem_regs[0] = 0; mem_regs[1] = 0; mem_regs[2] =  SIMD_N_REGS/2;
    flop_regs[0] = 1+SIMD_N_REGS/2; flop_regs[1] = 1+SIMD_N_REGS/2; flop_regs[2] = SIMD_N_REGS;
    flop_regnum = &(flop_regs[1]);
    for(i=0;i<n_ins;i++){
      dprint_MUOP(fd, mem_type, i, &offset, mem_regs[0], mem_regnum, mem_regs[2], "r11");
      dprint_FUOP(fd, flop_type, i, flop_regs[0], flop_regnum, flop_regs[2]);
    }
    mem_instructions = fop_instructions = n_ins;
  }
  else if(mop_per_fop > 1){
    if(mop_per_fop<SIMD_N_REGS){
      mem_regs[0] = 0; mem_regs[1] = 0; mem_regs[2] =  mop_per_fop;
      flop_regs[0] = 1+mop_per_fop; flop_regs[1] = 1+mop_per_fop; flop_regs[2] =  SIMD_N_REGS;
      flop_regnum = &(flop_regs[1]);
    }
    fop_instructions = 6;
    mem_instructions = fop_instructions * mop_per_fop;
    for(i=0;i<mem_instructions;i++){
      if(i%mop_per_fop==0){dprint_FUOP(fd, flop_type, i/mop_per_fop, flop_regs[0], flop_regnum, flop_regs[2]);}
      dprint_MUOP(fd, mem_type, i, &offset, mem_regs[0], mem_regnum, mem_regs[2], "r11");
    }
  }
  else if(fop_per_mop > 1){
    if(fop_per_mop<SIMD_N_REGS){
      flop_regs[0] = 0; flop_regs[1] = 0; flop_regs[2] =  fop_per_mop;
      mem_regs[0] = 1+fop_per_mop; mem_regs[1] = 1+fop_per_mop; mem_regs[2] =  SIMD_N_REGS;
      flop_regnum = &(flop_regs[1]);
    }
    
    mem_instructions = 6;
    fop_instructions = mem_instructions * fop_per_mop;
    for(i=0;i<fop_instructions;i++){
      if(i%fop_per_mop == 0) {dprint_MUOP(fd, mem_type, i, &offset, mem_regs[0], mem_regnum, mem_regs[2], "r11");}
      dprint_FUOP(fd, flop_type, i, flop_regs[0], flop_regnum, flop_regs[2]);
    }
  }
  dprint_oi_bench_end(fd, idx, offset);
  long flops = fop_instructions*SIMD_FLOPS;
  if(flop_type == ROOFLINE_FMA) flops *= 2;
  dprintf(fd, "out->instructions = in->loop_repeat * %u * in->stream_size / %lu;\n", mem_instructions+fop_instructions, offset);
  dprintf(fd, "out->flops = in->loop_repeat * %ld * in->stream_size / %lu;\n", flops, offset);
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
  int mem_type = type & (ROOFLINE_LOAD|ROOFLINE_LOAD_NT|ROOFLINE_STORE|ROOFLINE_STORE_NT|ROOFLINE_2LD1ST|ROOFLINE_COPY);
  int flop_type = type & (ROOFLINE_MUL|ROOFLINE_ADD|ROOFLINE_MAD|ROOFLINE_FMA);
  if(!mem_type  || ! flop_type){
    fprintf(stderr, "Both memory and compute type must be included in type\n");
    return NULL;
  }
  void (* benchmark) (const struct roofline_sample_in *, struct roofline_sample_out *, int);

  if(oi<=0){
    fprintf(stderr, "operational intensity must be greater than 0.\n");
    return NULL;
  }

  /* Create the filenames */
  snprintf(tempname, sizeof(tempname), "roofline_benchmark_%s_%s_oi_%lf", roofline_type_str(mem_type), roofline_type_str(flop_type), oi);
  len = strlen(tempname)+3; c_path =  malloc(len); memset(c_path,  0, len); snprintf(c_path, len, "%s.c", tempname);
  len = strlen(tempname)+4; so_path = malloc(len); memset(so_path, 0, len); snprintf(so_path, len, "%s.so", tempname);

  /* Generate benchmark */
  fd = open(c_path, O_WRONLY|O_CREAT, 0644);
  /* Write the appropriate roofline to the file */
  dprint_header(fd);
  oi_chunk_size = roofline_benchmark_write_oi_bench(fd, func_name, mem_type, flop_type, oi);
  /* Compile the roofline function */
  close(fd);
  /* char cmd[1024]; */
  /* snprintf(cmd, sizeof(cmd), "cat %s", c_path); */
  /* system(cmd); */
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

