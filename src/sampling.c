#include "roofline.h"
#include <papi.h>

#define roofline_rdtsc(c_high,c_low) __asm__ __volatile__ ("CPUID\n\tRDTSC\n\tmovq %%rdx, %0\n\tmovq %%rax, %1\n\t" :"=r" (c_high), "=r" (c_low)::"%rax", "%rbx", "%rcx", "%rdx")
#define roofline_rdtsc_diff(high, low) ((high << 32) | low)

char * output_name;
FILE * out;
int eventset = PAPI_NULL;
long long values[3];
uint64_t c_high_s, c_high_e, c_low_s, c_low_e;

#if defined (__AVX512__)
#define SIMD_BYTES         64
#elif defined (__AVX__)
#define SIMD_BYTES         32
#elif defined (__SSE__)
#define SIMD_BYTES         16
#endif

static void
PAPI_handle_error(int err)
{
    if(err!=0)
	fprintf(stderr,"PAPI error %d: ",err);
    switch(err){
    case PAPI_EINVAL:
	fprintf(stderr,"Invalid argument.\n");
	break;
    case PAPI_ENOINIT:
	fprintf(stderr,"PAPI library is not initialized.\n");
	break;
    case PAPI_ENOMEM:
	fprintf(stderr,"Insufficient memory.\n");
	break;
    case PAPI_EISRUN:
	fprintf(stderr,"Eventset is already_counting events.\n");
	break;
    case PAPI_ECNFLCT:
	fprintf(stderr,"This event cannot be counted simultaneously with another event in the monitor eventset.\n");
	break;
    case PAPI_ENOEVNT:
	fprintf(stderr,"This event is not available on the underlying hardware.\n");
	break;
    case PAPI_ESYS:
	fprintf(stderr, "A system or C library call failed inside PAPI, errno:%s\n",strerror(errno)); 
	break;
    case PAPI_ENOEVST:
	fprintf(stderr,"The EventSet specified does not exist.\n");
	break;
    case PAPI_ECMP:
	fprintf(stderr,"This component does not support the underlying hardware.\n");
	break;
    case PAPI_ENOCMP:
	fprintf(stderr,"Argument is not a valid component. PAPI_ENOCMP\n");
	break;
    case PAPI_EBUG:
	fprintf(stderr,"Internal error, please send mail to the developers.\n");
	break;
    default:
	perror("");
	break;
    }
}

#define PAPI_call_check(call,check_against, ...) do{	\
    int err = call;					\
    if(err!=check_against){				\
	fprintf(stderr, __VA_ARGS__);			\
	PAPI_handle_error(err);				\
	exit(EXIT_FAILURE);				\
    }							\
    } while(0)

void roofline_sampling_init(const char * output){
    printf("roofline sampling library initialization...\n");
    PAPI_call_check(roofline_lib_init(0),0, "roofline library initialization failed\n");
    out = fopen(output,"a+");
    if(out == NULL){
	perror("fopen");
	exit(EXIT_FAILURE);
    }
    output_name = (char*)output;
    PAPI_call_check(PAPI_library_init(PAPI_VER_CURRENT), PAPI_VER_CURRENT, "PAPI version mismatch\n");
    PAPI_call_check(PAPI_is_initialized(), PAPI_LOW_LEVEL_INITED, "PAPI initialization failed\n");
    printf("Powered by PAPI\n");
    PAPI_call_check(PAPI_create_eventset(&eventset), PAPI_OK, "PAPI eventset initialization failed\n");
    PAPI_call_check(PAPI_add_named_event(eventset, "MEM_UOPS_RETIRED:ALL_LOADS"), PAPI_OK,  "PAPI add named event %s failed\n", "MEM_UOPS_RETIRED:ALL_LOADS");
    PAPI_call_check(PAPI_add_named_event(eventset, "PAPI_DP_OPS"), PAPI_OK, "PAPI add named event %s failed\n", "PAPI_DP_OPS");
    PAPI_call_check(PAPI_add_named_event(eventset, "PAPI_TOT_INS"), PAPI_OK, "PAPI add named event %s failed\n", "PAPI_TOT_INS");
    printf("Consider each event MEM_UOPS_RETIRED:ALL_LOADS as a vectorized load micro-operation of %d bytes\n", SIMD_BYTES);
    printf("Trust in PAPI_DP_OPS for flops\n");
}

void roofline_sampling_fini(){
    fclose(out);
    PAPI_destroy_eventset(&eventset);
    printf("Output successfully written to %s\n", output_name);
}

void roofline_sampling_start(){
    PAPI_start(eventset);
    roofline_rdtsc(c_high_s, c_low_s);
}

void roofline_sampling_stop(){
    PAPI_stop(eventset,values);
    roofline_rdtsc(c_high_e, c_low_e);
    struct roofline_sample_out out_s = {				\
	roofline_rdtsc_diff(c_high_s, c_low_s),				\
	roofline_rdtsc_diff(c_high_e, c_low_e),				\
	values[2],							\
	values[0]*SIMD_BYTES,						\
	values[1]							\
    };
    hwloc_obj_t membind=NULL;
    hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();
    if(hwloc_get_membind(topology, nodeset, 0, HWLOC_MEMBIND_BYNODESET|HWLOC_MEMBIND_PROCESS) == -1){
	membind = hwloc_get_obj_by_type(topology,HWLOC_OBJ_NODE,0);
    }
    else{
	PAPI_call_check(hwloc_get_largest_objs_inside_cpuset(topology,nodeset,&membind, 1), -1, "hwloc_get_largest_objs_inside_cpuset");
    }
    hwloc_bitmap_free(nodeset);
    roofline_print_sample(out, membind, &out_s, 0, "MISC");
}

