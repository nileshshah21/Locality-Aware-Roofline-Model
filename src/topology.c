#include <hwloc.h>
#include "utils.h"
#include "roofline.h"
#include "topology.h"

#include "MSC/MSC.h"

void roofline_hwloc_check_version(){
#if HWLOC_API_VERSION >= 0x0020000
  /* Header uptodate for monitor */
  if(hwloc_get_api_version() < 0x20000)
    ERR_EXIT("hwloc version mismatch, required version 0x20000 or later, found %#08x\n", hwloc_get_api_version());
#else
  ERR_EXIT("hwloc version too old, required version 0x20000 or later\n");
#endif
}

int roofline_hwloc_get_memory_bounds(hwloc_obj_t memory, size_t * lower, size_t * upper, int op_type){
  hwloc_obj_t child = NULL;
  unsigned n_child = 1, n_memory = 1;
  
  /* Set lower bound size */
  child  = roofline_hwloc_get_under_memory(memory, root->type == HWLOC_OBJ_MACHINE);
  if(child != NULL){
    *lower = 2*roofline_hwloc_get_memory_size(child);
    n_child = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, child->type);
  }
  else{*lower = get_chunk_size(op_type)*n_threads;}

  /* Multiply number of child in case it would fit lower memory when splitting among threads */
  if(n_threads > 1) *lower *= n_child;

  /* Set upper bound size as memory size or MAX_SIZE */
  *upper = roofline_MIN(roofline_hwloc_get_memory_size(memory), max_size);
  n_memory = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, memory->type);
  if(n_threads > 1) *upper *= n_memory;

  if(*upper<*lower){
    if(child!=NULL){
      fprintf(stderr, "%s(%ld MB) above %s(%ld MB) is not large enough to be split into %u*%ld\n", 
	      hwloc_type_name(memory->type), (unsigned long)(roofline_hwloc_get_memory_size(memory)/1e6), 
	      hwloc_type_name(child->type), (unsigned long)(roofline_hwloc_get_memory_size(child)/1e6), 
	      n_child, (unsigned long)(roofline_hwloc_get_memory_size(child)/1e6));
    }
    else{
      fprintf(stderr, "minimum chunk size(%u*%lu B) greater than memory %s size(%lu B). Skipping.\n",
	      n_threads, get_chunk_size(op_type), 
	      hwloc_type_name(memory->type), roofline_hwloc_get_memory_size(memory));
    }
    return -1;
  }
  roofline_debug2("Memory bounds found at: [%ldB, %ldB]\n", *lower, *upper);
  return 0;
}

int roofline_hwloc_obj_snprintf(hwloc_obj_t obj, char * info_in, size_t n){
  int nc;
    
  memset(info_in,0,n);
  nc = hwloc_obj_type_snprintf(info_in, n, obj, 0);
  nc += snprintf(info_in+nc,n-nc,":%d ",obj->logical_index);
  return nc;
}

int roofline_hwloc_check_cpubind(hwloc_cpuset_t cpuset){
  int ret;
  hwloc_bitmap_t checkset = hwloc_bitmap_alloc();
  if(hwloc_get_cpubind(topology, checkset, HWLOC_CPUBIND_THREAD) == -1){
    perror("get_cpubind");
    hwloc_bitmap_free(checkset);  
    return 0; 
  }
#ifdef DEBUG2
    hwloc_obj_t bound = hwloc_get_first_largest_obj_inside_cpuset(topology, checkset);
    hwloc_obj_t bind = hwloc_get_first_largest_obj_inside_cpuset(topology, cpuset);
    fprintf(stderr, "membind=%s:%d, bound %s:%d\n",
	   hwloc_type_name(bind->type),bind->logical_index,
	   hwloc_type_name(bound->type),bound->logical_index);
#endif /* DEBUG2 */
  ret = hwloc_bitmap_isequal(cpuset,checkset);
  hwloc_bitmap_free(checkset);  
  return ret;
}

int roofline_hwloc_check_membind(hwloc_cpuset_t nodeset){
  hwloc_membind_policy_t policy;
  hwloc_bitmap_t checkset;
  int ret; 

  if(nodeset == NULL)
    return 0;
  
  checkset = hwloc_bitmap_alloc();
  
  if(hwloc_get_membind(topology, checkset, &policy, 0) == -1){
    perror("get_membind");
    hwloc_bitmap_free(checkset);  
    return 0; 
  }

#ifdef DEBUG2
    char * policy_name;
    switch(policy){
    case HWLOC_MEMBIND_DEFAULT:
      policy_name = "DEFAULT";
      break;
    case HWLOC_MEMBIND_FIRSTTOUCH:
      policy_name = "FIRSTTOUCH";
      break;
    case HWLOC_MEMBIND_BIND:
      policy_name = "BIND";
      break;
    case HWLOC_MEMBIND_INTERLEAVE:
      policy_name = "INTERLEAVE";
      break;
    case HWLOC_MEMBIND_NEXTTOUCH:
      policy_name = "NEXTTOUCH";
      break;
    case HWLOC_MEMBIND_MIXED:
      policy_name = "MIXED";
      break;
    default:
      policy_name=NULL;
      break;
    }
    hwloc_obj_t mem_obj = hwloc_get_first_largest_obj_inside_cpuset(topology, checkset);
    printf("membind(%s)=%s:%d\n",policy_name,hwloc_type_name(mem_obj->type),mem_obj->logical_index);
#endif /* DEBUG2 */

  ret = hwloc_bitmap_isequal(nodeset,checkset);
  hwloc_bitmap_free(checkset);  
  return ret;
}

inline int roofline_hwloc_objtype_is_cache(hwloc_obj_type_t type){
  return type==HWLOC_OBJ_L1CACHE || type==HWLOC_OBJ_L2CACHE || type==HWLOC_OBJ_L3CACHE || type==HWLOC_OBJ_L4CACHE || type==HWLOC_OBJ_L5CACHE;
}

hwloc_obj_t roofline_hwloc_parse_obj(char* arg){
  hwloc_obj_type_t type; 
  char * name;
  int err, depth; 
  char * idx;
  int logical_index;

  name = strtok(arg,":");

  if(name==NULL)
    return NULL;
    
  err = hwloc_type_sscanf_as_depth(name, &type, topology, &depth);
  if(err == HWLOC_TYPE_DEPTH_UNKNOWN){
    fprintf(stderr,"type %s cannot be found, level=%d\n",name,depth);
    return NULL;
  }
  if(depth == HWLOC_TYPE_DEPTH_MULTIPLE){
    fprintf(stderr,"type %s multiple caches match for\n",name);
    return NULL;
  }

  idx = strtok(NULL,":");
  logical_index = 0;
  if(idx!=NULL)
    logical_index = atoi(idx);
  return hwloc_get_obj_by_depth(topology,depth,logical_index);
}

int roofline_hwloc_cpubind(hwloc_obj_t obj){
  if(hwloc_set_cpubind(topology, obj->cpuset, HWLOC_CPUBIND_THREAD|HWLOC_CPUBIND_STRICT|HWLOC_CPUBIND_NOMEMBIND) == -1){
    perror("cpubind");
    return 0;
  }
  return roofline_hwloc_check_cpubind(obj->cpuset);
}

int roofline_hwloc_membind(hwloc_obj_t obj){
  if(obj->type == HWLOC_OBJ_NODE){
    if(hwloc_set_membind(topology,obj->nodeset,HWLOC_MEMBIND_BIND,HWLOC_MEMBIND_BYNODESET) == -1){
      perror("membind");
      return 0;
    }
  }
  else if(obj->type == HWLOC_OBJ_MACHINE){
    if(hwloc_set_membind(topology,obj->nodeset,HWLOC_MEMBIND_FIRSTTOUCH,HWLOC_MEMBIND_BYNODESET) == -1){
      perror("membind");
      return 0;
    }
  }
  else{return 1;}
  
  /* binding */
  return roofline_hwloc_check_membind(obj->cpuset);
}

int roofline_hwloc_obj_is_memory(hwloc_obj_t obj){
  hwloc_obj_cache_type_t type;
  
  if (obj->type == HWLOC_OBJ_NODE || obj->type == HWLOC_OBJ_MACHINE) return 1;
  if(roofline_hwloc_objtype_is_cache(obj->type)){
    type = obj->attr->cache.type;
    if(type == HWLOC_OBJ_CACHE_UNIFIED || type == HWLOC_OBJ_CACHE_DATA){
      return 1;
    }
  }
  return 0;
}

size_t roofline_hwloc_get_memory_size(hwloc_obj_t obj){
  hwloc_obj_cache_type_t type;
  if(obj==NULL)
    return 0;
  if ((int)obj->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NODE))
    return obj->memory.total_memory;
  if(roofline_hwloc_objtype_is_cache(obj->type)){
    type = obj->attr->cache.type;
    if(type == HWLOC_OBJ_CACHE_UNIFIED || type == HWLOC_OBJ_CACHE_DATA){
      return ((struct hwloc_cache_attr_s *)obj->attr)->size;
    }
  }
  return 0;
}


hwloc_obj_t roofline_hwloc_get_under_memory(hwloc_obj_t obj, int whole_system){
  hwloc_obj_t child;
  if(obj==NULL) return NULL;
  child = obj->first_child;
  
  /* Handle memory without child case */
  if(child == NULL && obj->type == HWLOC_OBJ_NODE){
    while(child == NULL && (obj = obj->prev_sibling) != NULL) child = obj->first_child;
  }

  /* go down to the first memory */
  while(child != NULL && !roofline_hwloc_obj_is_memory(child)) child = child->first_child;
  
  /* If we want whole system memory, we skip individual NUMANODES */
  if(child!=NULL && child->type == HWLOC_OBJ_NODE && whole_system)
    return roofline_hwloc_get_under_memory(child, whole_system);
  return child;
}


hwloc_obj_t roofline_hwloc_get_next_memory(hwloc_obj_t obj, int whole_system){
  /* If current_obj is not set, start from the bottom of the topology to return the first memory */
  if(obj == NULL) obj = hwloc_get_obj_by_depth(topology,hwloc_topology_get_depth(topology)-1,0);
  
  /* If current obj is a node, then next memory is a node at same depth (case where we dont look whole system)*/
  if(obj->type==HWLOC_OBJ_NODE && !whole_system) return obj->next_cousin;
  
  /* get parent memory */
  do{obj=obj->parent;} while(obj!=NULL && !roofline_hwloc_obj_is_memory(obj));

  /* We stopped on a memory subsystem or NULL. If we don't want a NUMANODE (case whole system) we skip it */
  if(obj != NULL && obj->type==HWLOC_OBJ_NODE && whole_system)
    return roofline_hwloc_get_next_memory(obj, whole_system);
  
  /* No memory left in topology */
  return obj;
}
