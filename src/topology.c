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
  hwloc_obj_t child = memory;
  unsigned n_child = 1, n_mem;
  size_t child_size, mem_size = roofline_hwloc_get_memory_size(memory);

  /* Variable to bound size in order to avoid swap */
  unsigned node_depth = (unsigned)hwloc_get_type_depth(topology, HWLOC_OBJ_NODE);
  hwloc_obj_t first_node = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NODE, 0);
  size_t max_size = roofline_hwloc_get_memory_size(first_node) / 4;
  
  /* Set lower bound size according to child caches */
  do{
    child  = roofline_hwloc_get_under_memory(child);
  } while(child!=NULL && child->depth <= node_depth);
  
  if(child == NULL) *lower = get_chunk_size(op_type)*n_threads;
  else{
    child_size = roofline_hwloc_get_memory_size(child);
    n_child = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, child->type);
    n_child = n_threads>1 ? roofline_MAX(1,n_child) : 1;
    if(memory->depth <= node_depth) *lower = 4 * child_size * n_child;
    else                            *lower = 2 * child_size * n_child;
  }

  /* Set upper bound size as memory size or MAX_SIZE */
  n_mem = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, memory->type);
  n_mem = n_threads>1 ? roofline_MAX(1,n_mem) : 1;
  if(memory->depth <= node_depth) *upper = roofline_MIN(max_size/8,*lower*32);
  else *upper = mem_size * n_mem;

#ifdef DEBUG2
  roofline_mkstr(target, 128);
  roofline_hwloc_obj_snprintf(memory, target, sizeof(target));
  printf("Memory bounds for %s:%d found at: [%ldB, %ldB]\n", target, memory->logical_index, *lower, *upper);
#endif
  
  /* Handle unexpected sizes */
  if(*upper<*lower){
    if(child!=NULL){
      fprintf(stderr, "%s(%ld KB) above %s(%ld KB) is not large enough to be split into %u*%ld\n", 
	      hwloc_type_name(memory->type), *upper, 
	      hwloc_type_name(child->type), (unsigned long)(roofline_hwloc_get_memory_size(child)/1e3), 
	      n_child, (unsigned long)(roofline_hwloc_get_memory_size(child)/1e3));
    }
    else{
      fprintf(stderr, "minimum chunk size(%u*%lu B) greater than memory %s size(%lu B). Skipping.\n",
	      n_threads, get_chunk_size(op_type), 
	      hwloc_type_name(memory->type), roofline_hwloc_get_memory_size(memory));
    }
    return -1;
  }
  
  return 0;
}

int roofline_hwloc_obj_snprintf(hwloc_obj_t obj, char * info_in, size_t n){
  int nc;
    
  memset(info_in,0,n);
  nc = hwloc_obj_type_snprintf(info_in, n, obj, 0);
  if((int)obj->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NODE)) nc += snprintf(info_in+nc,n-nc,":%d ",obj->logical_index);
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
    fprintf(stderr, "cpubind=%s:%d, bound %s:%d\n",
	   hwloc_type_name(bind->type),bind->logical_index,
	   hwloc_type_name(bound->type),bound->logical_index);
#endif /* DEBUG2 */
  ret = hwloc_bitmap_isequal(cpuset,checkset);
  hwloc_bitmap_free(checkset);  
  return ret;
}

#ifdef DEBUG2
static void roofline_hwloc_print_area_membind(hwloc_obj_t membind_location, void * ptr, size_t size){
  hwloc_bitmap_t area = hwloc_bitmap_alloc();
  hwloc_membind_policy_t policy;

  if(hwloc_get_area_membind(topology, ptr, size, area, &policy, 0) == -1){
    perror("hwloc_get_area_membind");
    goto print_area_error;
  }
  hwloc_obj_t location;
  if(hwloc_get_largest_objs_inside_cpuset(topology, area, &location, 1) == -1){
    perror("hwloc_get_largest_objs_inside_cpuset");
    goto print_area_error;
  }
  roofline_mkstr(area_str,128);
  roofline_mkstr(target,128);
  roofline_hwloc_obj_snprintf(membind_location, target, sizeof(target));
  if(location != NULL) roofline_hwloc_obj_snprintf(location, area_str, sizeof(area_str));
  printf("area allocation on %s, allocated on %s\n", target, area_str);

  return;
print_area_error:
  hwloc_bitmap_free(area);
}
#endif

int roofline_hwloc_set_area_membind(hwloc_obj_t membind_location, void * ptr, size_t size){
  /* Bind on node */
  if(membind_location != NULL     &&
     membind_location->depth != 0 &&
     (int)membind_location->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NODE))
  {
    hwloc_membind_policy_t policy = HWLOC_MEMBIND_BIND;
    unsigned n_nodes = hwloc_get_nbobjs_inside_cpuset_by_type(topology, membind_location->cpuset, HWLOC_OBJ_NODE);
    /* If there are several NUMA memory possible, we have to choose among them where to bind memory. */
    if(n_nodes>1){
      /* Balance with interleave on remote nodes. More efficient than balancing as for local nodes */
      if(!hwloc_bitmap_isincluded(membind_location->cpuset, root->cpuset)){
	policy = HWLOC_MEMBIND_INTERLEAVE;
      }
      /* Balance round robin per group of threads (NUMA:0 <- group:0, NUMA:1 <- group:1) */
      else {	  
	unsigned tid = 0;
#ifdef _OPENMP
	tid = omp_get_thread_num();
#endif
	membind_location = hwloc_get_obj_inside_cpuset_by_type(topology, membind_location->cpuset, HWLOC_OBJ_NODE,
							       tid*n_nodes/n_threads);
      }
    }
    /* Apply memory binding */
    if(hwloc_set_area_membind(topology, ptr, size, membind_location->nodeset,policy,
			      HWLOC_MEMBIND_THREAD   |
			      HWLOC_MEMBIND_NOCPUBIND|
			      HWLOC_MEMBIND_STRICT   |
			      HWLOC_MEMBIND_MIGRATE  |
			      HWLOC_MEMBIND_BYNODESET) == -1){
      perror("hwloc_set_area_membind");
      return 0;
    }
  }
  /* Bind on Close to thread */
  else {
    hwloc_obj_t thread_location;
    hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    if(hwloc_get_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD) == -1){
      perror("get_cpubind");
      goto machine_bind_error;
    }
    if(hwloc_get_largest_objs_inside_cpuset(topology, cpuset, &thread_location, 1) == -1){
      perror("hwloc_get_largest_objs_inside_cpuset");
      goto machine_bind_error;
    }
    while(thread_location != NULL && thread_location->type != HWLOC_OBJ_NODE) thread_location = thread_location->parent;
    return roofline_hwloc_set_area_membind(thread_location, ptr, size);

  machine_bind_error:
    hwloc_bitmap_free(cpuset);
    return 0; 
  }

  /* Print area location */
#ifdef DEBUG2
  roofline_hwloc_print_area_membind(membind_location, ptr, size);
#endif
  memset(ptr, 0, size);
  return 1;
}


inline int roofline_hwloc_objtype_is_cache(hwloc_obj_type_t type){
  return type==HWLOC_OBJ_L1CACHE || type==HWLOC_OBJ_L2CACHE || type==HWLOC_OBJ_L3CACHE || type==HWLOC_OBJ_L4CACHE || type==HWLOC_OBJ_L5CACHE;
}

hwloc_obj_t roofline_hwloc_parse_obj(const char* arg){
  hwloc_obj_type_t type; 
  char * name;
  int err, depth; 
  char * idx;
  int logical_index;
  char * dup_arg = strdup(arg);
  name = strtok(dup_arg,":");

  if(name==NULL) goto err_parse_obj;
    
  err = hwloc_type_sscanf_as_depth(name, &type, topology, &depth);
  if(err == HWLOC_TYPE_DEPTH_UNKNOWN){
    fprintf(stderr,"type %s cannot be found, level=%d\n",name,depth);
    goto err_parse_obj;
  }
  if(depth == HWLOC_TYPE_DEPTH_MULTIPLE){
    fprintf(stderr,"type %s multiple caches match for\n",name);
    goto err_parse_obj;
  }

  idx = strtok(NULL,":");
  logical_index = 0;
  if(idx!=NULL) logical_index = atoi(idx);
  free(dup_arg);
  return hwloc_get_obj_by_depth(topology,depth,logical_index);

err_parse_obj:
  free(dup_arg);
  return NULL;
}

int roofline_hwloc_cpubind(hwloc_obj_type_t leaf_type){
  unsigned tid = 0;
#ifdef _OPENMP
  tid = omp_get_thread_num();
#endif
  unsigned n_leaves = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, leaf_type);
  hwloc_obj_t leaf = hwloc_get_obj_inside_cpuset_by_type(topology, root->cpuset, leaf_type, tid%n_leaves);
  if(leaf_type == HWLOC_OBJ_CORE) leaf = leaf->first_child;
  
  if(hwloc_set_cpubind(topology, leaf->cpuset, HWLOC_CPUBIND_THREAD|HWLOC_CPUBIND_STRICT|HWLOC_CPUBIND_NOMEMBIND) == -1){
    perror("cpubind");
    return 0;
  }
  
  return roofline_hwloc_check_cpubind(leaf->cpuset);
}

int roofline_hwloc_obj_is_memory(hwloc_obj_t obj){
  hwloc_obj_cache_type_t type;
  
  if ((int)obj->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NODE)) return 1;
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


hwloc_obj_t roofline_hwloc_get_under_memory(hwloc_obj_t obj){
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
  if(child!=NULL &&
     (int)root->depth < hwloc_get_type_depth(topology, HWLOC_OBJ_NODE) &&
     (int)child->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NODE))
    return roofline_hwloc_get_under_memory(child);
  return child;
}


hwloc_obj_t roofline_hwloc_get_next_memory(hwloc_obj_t obj){
  /* If current_obj is not set, start from the bottom of the topology to return the first memory */
  if(obj == NULL) obj = hwloc_get_obj_by_depth(topology,hwloc_topology_get_depth(topology)-1,0);
  
  /* If current obj is not a cache, then next memory is at same depth in root cpuset (if any) */
  if((int)obj->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NODE) && obj->next_cousin != NULL) return obj->next_cousin;
  
  /* get parent memory */
  do{obj=hwloc_get_obj_inside_cpuset_by_depth(topology, root->cpuset, obj->depth-1, 0);} while(obj!=NULL && !roofline_hwloc_obj_is_memory(obj));
  
  /* No memory left in topology */
  return obj;
}

