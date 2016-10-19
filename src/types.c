#include <hwloc.h>
#include "types.h"
#include "MSC/MSC.h"

int roofline_type_from_str(const char * type){
  if(!strcmp(type, "LOAD") || !strcmp(type, "load")){return ROOFLINE_LOAD;}
  if(!strcmp(type, "LOAD_NT") || !strcmp(type, "load_nt")){return ROOFLINE_LOAD_NT;}
  if(!strcmp(type, "STORE") || !strcmp(type, "store")){return ROOFLINE_STORE;}
  if(!strcmp(type, "STORE_NT") || !strcmp(type, "store_nt")){return ROOFLINE_STORE_NT;}
  if(!strcmp(type, "2LD1ST") || !strcmp(type, "2ld1st")){return ROOFLINE_2LD1ST;}
  if(!strcmp(type, "COPY") || !strcmp(type, "copy")){return ROOFLINE_COPY;}
  if(!strcmp(type, "ADD") || !strcmp(type, "add")){return ROOFLINE_ADD;}
  if(!strcmp(type, "MUL") || !strcmp(type, "mul")){return ROOFLINE_MUL;}
  if(!strcmp(type, "MAD") || !strcmp(type, "mad")){return ROOFLINE_MAD;}
  if(!strcmp(type, "FMA") || !strcmp(type, "fma")){return ROOFLINE_FMA;}
  return -1;
}

const char * roofline_type_str(int type){
  if(type & ROOFLINE_LOAD) return "load";
  if(type & ROOFLINE_LOAD_NT) return "load_nt";
  if(type & ROOFLINE_STORE) return "store";
  if(type & ROOFLINE_STORE_NT) return "store_nt";
  if(type & ROOFLINE_2LD1ST) return "2LD1ST";
  if(type & ROOFLINE_COPY) return "copy";
  if(type & ROOFLINE_ADD) return "ADD";
  if(type & ROOFLINE_MUL) return "MUL";
  if(type & ROOFLINE_MAD) return "MAD";
  if(type & ROOFLINE_FMA) return "FMA";
  return "";
}

int roofline_filter_types(hwloc_obj_t obj, int type){
  int supported = benchmark_types_supported();
  int FP_possible = (ROOFLINE_ADD|ROOFLINE_MAD|ROOFLINE_MUL|ROOFLINE_FMA) & supported;
  int CACHE_possible = (ROOFLINE_LOAD|ROOFLINE_STORE|ROOFLINE_2LD1ST|ROOFLINE_COPY) & supported;
  int NUMA_possible = (CACHE_possible|ROOFLINE_LOAD_NT|ROOFLINE_STORE_NT) & supported;
  int obj_type = 0;
  
  if(obj->type == HWLOC_OBJ_L1CACHE){
    if(type & ROOFLINE_LOAD_NT) fprintf(stderr, "skip load_nt type not meaningful for %s\n", hwloc_type_name(obj->type));
    if(type & ROOFLINE_STORE_NT) fprintf(stderr, "skip store_nt type not meaningful for %s\n", hwloc_type_name(obj->type));
    obj_type = type & CACHE_possible;
    if(obj_type == 0) obj_type = (ROOFLINE_LOAD|ROOFLINE_STORE|ROOFLINE_2LD1ST) & supported;
  }
    
  else if(obj->type == HWLOC_OBJ_L2CACHE ||
	  obj->type == HWLOC_OBJ_L3CACHE ||
	  obj->type == HWLOC_OBJ_L4CACHE ||
	  obj->type == HWLOC_OBJ_L5CACHE){
    if(type & ROOFLINE_LOAD_NT) fprintf(stderr, "skip load_nt type not meaningful for %s\n", hwloc_type_name(obj->type));
    if(type & ROOFLINE_STORE_NT) fprintf(stderr, "skip store_nt type not meaningful for %s\n", hwloc_type_name(obj->type));
    obj_type = type & CACHE_possible;
    if(obj_type == 0) obj_type = (ROOFLINE_STORE|ROOFLINE_LOAD) & supported;
  }

  else if(obj->type == HWLOC_OBJ_NUMANODE || obj->type == HWLOC_OBJ_MACHINE){    
    obj_type = type & NUMA_possible;
    if(obj_type == 0) obj_type = (ROOFLINE_STORE_NT|ROOFLINE_LOAD_NT|ROOFLINE_STORE|ROOFLINE_LOAD) & supported;
  }

  else if (obj->type == HWLOC_OBJ_PU || obj->type == HWLOC_OBJ_CORE){
    obj_type = type & FP_possible;
    if(obj_type == 0) obj_type = (ROOFLINE_ADD|ROOFLINE_MAD) & supported;
  }

  return obj_type;
}

