#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include "arm.h"
#include "../../stats.h"
#include "../../roofline.h"

/* POWER CARM EXTENSION*/
#include <stdlib.h>     
#include "../../utils.h"

extern size_t oi_chunk_size;
static const unsigned reg_index[32] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
/* static const unsigned reg_index[32] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14}; */
				   

/***************************************** OI BENCHMARKS GENERATION ******************************************/
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
static void dprint_FUOP_by_ins(int fd, const char * op, unsigned * regnum){
  // Assuming using scalar operations for simplicity; adjust for vector operations as needed.
  dprintf(fd, "\"%s v%d.4s, v%d.4s, v%d.4s\\n\\t\"\\\n", op, *regnum, *regnum, *regnum);
}
#else
static void dprint_FUOP_by_ins(int fd, const char * op, unsigned * regnum){
  // For non-NEON, fallback or scalar operation; unlikely needed but for structural symmetry.
  dprintf(fd, "\"%s v%d.4s, v%d.4s\\n\\t\"\\\n", op, *regnum, *regnum);
}
#endif

static void dprint_FUOP(int fd, int type, unsigned * i, unsigned * regnum){
  switch(type){
  case ROOFLINE_ADD:
    dprint_FUOP_by_ins(fd, "fadd", regnum);
    break;
  case ROOFLINE_MUL:
    dprint_FUOP_by_ins(fd, "fmul", regnum);
    break;
  case ROOFLINE_MAD:
    // ARM NEON does not have a direct MAD instruction; alternating between ADD and MUL for illustration.
    if((*i)%2) dprint_FUOP_by_ins(fd, "fmul", regnum);
    else dprint_FUOP_by_ins(fd, "fadd", regnum);
    break;
#if defined(__ARM_FEATURE_FMA)
  case ROOFLINE_FMA:
    dprint_FUOP_by_ins(fd, "fmla", regnum); // Assuming FMLA for FMA operation
    break;
#endif
  default:
    break;
  }
  *regnum = (*regnum+1)%32;
  *i += 1;
}

static void dprint_MUOP(int fd, int type, unsigned *i, off_t * offset, unsigned * regnum){
  switch(type){
  case ROOFLINE_LOAD:
    dprintf(fd, "\"ld1 {v%d.4s}, [x11, #%lu]\\n\\t\"\\\n", *regnum, *offset);
    break;
  case ROOFLINE_STORE:
    dprintf(fd, "\"st1 {v%d.4s}, [x11, #%lu]\\n\\t\"\\\n", *regnum, *offset);
    break;
  // ARM does not directly support non-temporal loads/stores in the same way as x86, omitting ROOFLINE_LOAD_NT and ROOFLINE_STORE_NT.
  case ROOFLINE_2LD1ST:
    // ARM NEON equivalent logic for 2LD1ST not provided due to complexity.
    break;
  default:
    break;
  }
  
  *offset += 16; // NEON registers are typically 128 bits (16 bytes).
  *regnum = (*regnum+1)%32;
  *i += 1;
}

static void dprint_zero_simd(int fd){
  dprintf(fd, "static void zero_simd(){\n");
  dprintf(fd, "__asm__ __volatile__ (\\\n\t");
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_0));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_1));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_2));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_3));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_4));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_5));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_6));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_7));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_8));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_9));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_10));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_11));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_12));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_13));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_14));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_15));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_16));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_17));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_18));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_19));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_20));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_21));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_22));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_23));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_24));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_25));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_26));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_27));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_28));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_29));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_30));
  dprintf(fd, "\"%s\\n\\t\"\\\n\t", zero_reg(REG_31));
  dprintf(fd,"::: \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"memory\" );\n}\n\n", SIMD_CLOBBERED_REGS);
}

static void  dprint_header(int fd) {
  dprintf(fd, "#include <stdlib.h>\n");
  dprintf(fd, "#include \"stream.h\"\n");
  dprintf(fd, "#include \"output.h\"\n");
  dprintf(fd, "#include \"utils.h\"\n");

  /* in order to introduce hardware memory barriers */
  // dprintf(fd, "#include <immintrin.h>\n ");/* POWER CARM EXTENSION*/  
  
  dprint_zero_simd(fd);
}

static void dprint_globaldecls(int fd, unsigned int buffer_size) /* POWER CARM EXTENSION*/
{
  /* Aim of this function is to generate reuqired #define's in benchmark code */
  /* TODO----    Right now it is not "general" enough               ----- */
  /*since we wnat to read MSR values , we need to use popen() calls which can redirect stdout output to a desired file */
  /* to read that string iput , a buffer size for buffer is defined */

 // const char* define1=CONCATENATE("#define BUFFER_SIZE",STRINGIFY(buffer_size));
 // define1=CONCATENATE(define1,"\n");
  dprintf(fd,"#define BUFFER_SIZE 1024\n");
  //const char* define2= CONCATENATE("#define ROCKETLAKE_ENERGY_UNIT",STRINGIFY(buffer_size));
  dprintf(fd,"const char* command=\"sudo rdmsr -d 1553\";\n");
  dprintf(fd,"unsigned long int msr_val_start=0;\n");
  dprintf(fd,"FILE* temp_start;\n");
  dprintf(fd,"char buffer_start[BUFFER_SIZE];\n");
  dprintf(fd,"char *endptr_start;\n");
  dprintf(fd,"unsigned long int msr_val_end=0;\n");
  dprintf(fd,"FILE* temp_end;\n");
  dprintf(fd,"char buffer_end[BUFFER_SIZE];\n");
  dprintf(fd,"char *endptr_end;\n");


}


static void hardware_memory_barrier(int fd) /* POWER CARM EXTENSION*/
{
 /* Aim of this function is to put some memory barier calls in the generated bencmark*/
 dprintf(fd,"_mm_mfence();\n");

}

static void msr_to_buffer_begin(int fd )  /* POWER CARM EXTENSION*/
{
    /* Aim of this function is to run "command" and read the output , store it to buffer */
    
    dprintf(fd,"\n\n");
    hardware_memory_barrier(fd);
    
    dprintf(fd,"temp_start=popen(command, \"r\");\n"); /* intializing a pipe*/
    dprintf(fd,"if (temp_start == NULL) { \n perror(\"Err in Popen while reading MSR\");\n }\n");   
    dprintf(fd,"fread(buffer_start, 1, 30, temp_start);\n");
    dprintf(fd,"pclose(temp_start);\n");    
    dprintf(fd,"msr_val_start = strtoul(buffer_start, &endptr_start, 10);\n");
    hardware_memory_barrier(fd);
    dprintf(fd,"printf(\"@@@@@@@\");\n");
    //dprintf(fd,"printf(\"%lu\",msr_val_start);\n");
    dprintf(fd,"power_roofline_output_begin_measure(out,msr_val_start);\n");
    dprintf(fd,"\n\n");
} 

static void msr_to_buffer_end(int fd )  /* POWER CARM EXTENSION*/
{
    /* Aim of this function is to run "command" and read the output , store it to buffer */
    
    dprintf(fd,"\n\n");
    hardware_memory_barrier(fd);
    
    dprintf(fd,"temp_end=popen(command, \"r\");\n"); /* intializing a pipe*/
    dprintf(fd,"if (temp_end == NULL) { \n perror(\"Err in Popen while reading MSR\");\n }\n");    
    dprintf(fd,"fread(buffer_end, 1, 30, temp_end);\n");
    dprintf(fd,"pclose(temp_end);\n");    
    dprintf(fd,"msr_val_end = strtoul(buffer_end, &endptr_end, 10);\n");

    hardware_memory_barrier(fd);

    dprintf(fd,"printf(\"@@@@@@@\");\n");
    //dprintf(fd,"printf(\"%lu \",msr_val_end);\n");
    dprintf(fd,"power_roofline_output_end_measure(out,msr_val_end);\n");
     dprintf(fd,"\n\n");
} 


// static void msr_read_command(int fd) /* POWER CARM EXTENSION*/
// {
//   dprintf(fd,"const char* command=\"sudo rdmsr -d 1553\";\n");

// }

static void buffer_reintialize(int fd)
{
  
  dprintf(fd,"\n\n /* ****** reintializing string and integer buffers ****** */ \n \n");
  dprintf(fd,"msr_val_start=0;\n");
  dprintf(fd,"temp_start=0;\n");
  dprintf(fd,"memset(buffer_start,0,sizeof(buffer_start));\n");
  dprintf(fd,"memset(buffer_end,0,sizeof(buffer_end));\n");
  dprintf(fd,"endptr_start=0;\n");
  dprintf(fd,"msr_val_end=0;\n");
  dprintf(fd,"temp_end=0;\n");
  dprintf(fd,"endptr_end=0;\n");
  dprintf(fd,"msr_val_end=0;\n");

  //dprintf(fd,"printf(\"HELLO\");\n");

}

static void dprint_oi_bench_begin(int fd, const char * id){
  // Preparing for measurement start. Adapted for ARM Cortex-A57.
  // msr_to_buffer_begin(fd);

  dprintf(fd, "roofline_output_begin_measure(out);\n");
  dprintf(fd, "__asm__ __volatile__ (\\\n");
  dprintf(fd, "\"loop_%s_repeat:\\n\\t\"\\\n", id);
  dprintf(fd, "\"ldr x11, %%1\\n\\t\"\\\n"); // Load address/counter into x11
  dprintf(fd, "\"ldr x12, %%2\\n\\t\"\\\n"); // Load data/stream size into x12
  dprintf(fd, "\"buffer_%s_increment:\\n\\t\"\\\n", id); // Loop label for benchmark
}

static void dprint_oi_bench_end(int           fd,
                                const char *  id,
                                off_t         offset,
                                unsigned long bytes,
                                unsigned long flops,
                                unsigned long ins){
  dprintf(fd,"\"add x11, x11, #%lu\\n\\t\"\\\n", offset); // Adjust pointer/data address
  dprintf(fd,"\"sub x12, x12, #%lu\\n\\t\"\\\n", offset); // Decrement counter/size
  dprintf(fd,"\"cbnz x12, buffer_%s_increment\\n\\t\"\\\n", id); // Conditional branch if not zero
  dprintf(fd,"\"subs %%0, %%0, #1\\n\\t\"\\\n"); // Decrement loop counter
  dprintf(fd,"\"cbnz %%0, loop_%s_repeat\\n\\t\"\\\n", id); // Conditional branch if not zero
  dprintf(fd,":: \"r\" (%s), \"r\" (data->stream), \"r\" (data->size)\\\n", (bytes==0?"1":"repeat"));
  dprintf(fd,": \"memory\", \"x11\", \"x12\" );\n"); // Clobbered registers and memory
  if(bytes == 0){ 
    dprintf(fd,"roofline_output_end_measure(out, 0, 0, 0);\n"); 
  } else {
    dprintf(fd,"roofline_output_end_measure(out, repeat*data->size, repeat*data->size*%lu/%lu, repeat*data->size*%lu/%lu);\n",
            flops, bytes, ins, bytes); 
  }
  // msr_to_buffer_end(fd);
  // buffer_reinitialize(fd);
}


 
static int roofline_compile_lib(char * c_path, char* so_path){
  char cmd[2048];
  printf("********************************************************************************");
  printf("********************* c_path is %s *********************************\n",c_path);
  //printf("enter dummy integer input \n");
  //int dummy;
  //scanf("%d",&dummy);
#ifdef DEBUG1
  memset(cmd,0,sizeof(cmd));
  snprintf(cmd, sizeof(cmd), "cat %s", c_path);
  system(cmd);
#endif
#define DEBUG1
  memset(cmd,0,sizeof(cmd));
  sprintf(cmd, "%s -v -g -fPIC -shared %s -rdynamic -o %s roofline.so -fopenmp", STRINGIFY(CC), c_path, so_path);
  printf("*******??????????????^^^^^^^^^^^^^^\n");
  printf("*******??????????????^^^^^^^^^^^^^^\n\n");
  printf("%s\n",cmd);
  printf("*******??????????????^^^^^^^^^^^^^^\n");
  printf("*******??????????????^^^^^^^^^^^^^^\n\n");

  return system(cmd);
}

off_t roofline_benchmark_write_oi_bench(int fd, const char * name, int mem_type, int flop_type, unsigned mem_ins, unsigned flop_ins){
  
  off_t offset = 0; size_t len;
  unsigned i;
  unsigned muops = 0, fuops = 0, regnum = 0;
  char * idx;
  unsigned long bytes = 0;
  unsigned long flops = 0;
  unsigned long ins = 0;
  /* mem_ins*=2; */
  /* flop_ins*=2;   */
  len = 3+strlen(roofline_type_str(mem_type))+strlen(roofline_type_str(flop_type))+strlen("validation");
  idx = malloc(len);
  dprintf(fd, "void %s(roofline_stream data, roofline_output out, __attribute__ ((unused)) int op_type, long repeat){\n", name);
  dprintf(fd, "zero_simd();\n");
  
  memset(idx,0,len);
  snprintf(idx,len,"%s_%s", roofline_type_str(mem_type), roofline_type_str(flop_type));
  dprint_oi_bench_begin(fd, idx);
  do{
    for(i=0; i<mem_ins; i++) dprint_MUOP(fd, mem_type, &muops, &offset, &regnum);
    for(i=0; i<flop_ins; i++) dprint_FUOP(fd, flop_type, &fuops, &regnum);
    bytes += mem_ins  * SIMD_BYTES;
    flops += flop_ins * SIMD_FLOPS * (flop_type==ROOFLINE_FMA?2:1);
    ins   += flop_ins + mem_ins;
    if(ins>64 && regnum>=(unsigned)(SIMD_N_REGS/2) && (mem_type==ROOFLINE_2LD1ST?!(muops%3):1)){ break; }
  } while(regnum!=0 || (mem_type==ROOFLINE_2LD1ST?muops%3:0));
  dprint_oi_bench_end(fd, idx, offset, bytes, flops, ins);

  /* overhead measure */
  memset(idx,0,len);
  snprintf(idx,len,"%s_%s_%s", roofline_type_str(mem_type), roofline_type_str(flop_type), "validation");
  dprint_oi_bench_begin(fd, idx);
  dprint_oi_bench_end(fd, idx, offset, 0, 0, 0);

  dprintf(fd, "}\n\n");
  close(fd);
  free(idx);
  return offset;
}

static void * roofline_load_lib(const char * path, const char * funname){
  void * benchmark, * handle;
    
  dlerror();
  handle = dlopen(path, RTLD_NOW|RTLD_DEEPBIND);
  if(handle == NULL){
    ERR("Failed to load dynamic library %s: %s\n", path, dlerror());
    return NULL;
  }

  dlerror();
  benchmark = dlsym(handle, funname);
  if(benchmark == NULL){
    ERR("Failed to load function %s from %s: %s\n", funname, path, dlerror());
  }
  return benchmark;
}

void * benchmark_validation(int op_type, unsigned flops, unsigned bytes){
  const char * func_name = "roofline_oi_benchmark";
  char tempname[1024], * c_path, * so_path;
  size_t len;
  int fd;
  void * benchmark = NULL;
  
  int mem_type = op_type & (ROOFLINE_LOAD|ROOFLINE_STORE);
  int flop_type = op_type & (ROOFLINE_MUL|ROOFLINE_ADD);  
  /* Parameters checking */
  if(!mem_type  || ! flop_type){
    ERR("Both memory and compute type must be included in type\n");
    return NULL;
  }
  if(bytes == 0 || flops == 0){
    ERR("bytes and flops cant be 0.\n");
    return NULL;
  }

  /* Convert flops and bytes into instruction number */
  unsigned mem_ins=bytes, flop_ins = flops*SIMD_BYTES/SIMD_FLOPS;
  if(flop_type == ROOFLINE_FMA){mem_ins*=2;}
  unsigned pgcd = roofline_PGCD(flop_ins, mem_ins);
  mem_ins /= pgcd; flop_ins/=pgcd;
  
  roofline_debug2("benchmark validation for %u bytes and %u flops will interleave %u memory instructions with %u flop instructions\n", bytes, flops, mem_ins, flop_ins);
  
  /* Create the filenames */
  snprintf(tempname, sizeof(tempname), "roofline_benchmark_%s_%uB_%s_%uFlops",
	   roofline_type_str(mem_type), bytes,
	   roofline_type_str(flop_type), flops);
  len = strlen(tempname)+3; c_path =  malloc(len); memset(c_path,  0, len); snprintf(c_path, len, "%s.c", tempname);
  len = strlen(tempname)+4; so_path = malloc(len); memset(so_path, 0, len); snprintf(so_path, len, "%s.so", tempname);

  /* Generate benchmark */
  fd = open(c_path, O_WRONLY|O_CREAT, 0644);
  /* Write the appropriate roofline to the file */
  dprint_header(fd);
  oi_chunk_size = roofline_benchmark_write_oi_bench(fd, func_name, mem_type, flop_type, mem_ins, flop_ins);
  /* Compile the roofline function */
  close(fd);
  roofline_compile_lib(c_path, so_path);
  /* Load the roofline function */
  benchmark = roofline_load_lib(so_path, func_name);

#ifdef DEUBG2
  roofline_mkstr(cmd, 1024);
  snprintf(cmd, sizeof(cmd), "cat %s", c_path);
  system(cmd);  
#endif
  
  unlink(c_path);
  unlink(so_path);
  free(c_path);
  free(so_path);
  return benchmark;
}