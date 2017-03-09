#include <hwloc.h>
#include "utils.h"
#include "roofline.h"
#include "topology.h"
#include "utils.h"
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

int roofline_hwloc_get_memory_bounds(const hwloc_obj_t memory, size_t * lower, size_t * upper, const int op_type){
  hwloc_obj_t child = memory;
  unsigned n_child = 1, n_mem;
  size_t child_size, mem_size = roofline_hwloc_get_memory_size(memory);

  /* Variable to bound size in order to avoid swap */
  hwloc_obj_t first_node = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, 0);
  size_t max_size = roofline_hwloc_get_memory_size(first_node) / 2;
  
  /* Set lower bound size according to child caches */
  do{
    child  = roofline_hwloc_get_under_memory(child);
  } while(child!=NULL && child->depth <= first_node->depth);
  if(child == NULL) {
    *lower = get_chunk_size(op_type)*n_threads;
  } else{    
    child_size = roofline_hwloc_get_memory_size(child);
    n_child = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, child->type);
    n_child = n_threads>1 ? roofline_MAX(1,n_child) : 1;
    *lower = 2*child_size*n_child;
  }

  /* Set upper bound size as memory size/2 or MAX_SIZE */
  n_mem = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, memory->type);
  n_mem = n_threads>1 ? roofline_MAX(1,n_mem) : 1;
  *upper = mem_size * n_mem / 2;
  *upper = roofline_MIN(*upper, max_size);

  /* Shrink if possible to save time */
  if(*upper > *lower*4){*upper = *upper/4;}
  else if(*upper > *lower*2){*upper = *upper/2;}

#ifdef DEBUG2
  roofline_mkstr(target, 128);
  roofline_hwloc_obj_snprintf(memory, target, sizeof(target));
  printf("Memory bounds for %s found at: [%ldB, %ldB]\n", target, *lower, *upper);
#endif
  
  /* Handle unexpected sizes */
  if(*upper<*lower){
    if(child!=NULL){
      fprintf(stderr, "%s(%ld KB) above %s(%ld KB) is not large enough to be split into %u*%ld\n", 
	      hwloc_type_name(memory->type), (unsigned long)(*upper/1e3), 
	      hwloc_type_name(child->type), (unsigned long)(child_size/1e3), 
	      n_child, (unsigned long)(child_size/1e3));
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

int roofline_hwloc_obj_snprintf(const hwloc_obj_t obj, char * info_in, const size_t n){
  int nc;
  memset(info_in,0,n);
  /* Special case for MCDRAM */
  if(obj->type == HWLOC_OBJ_NUMANODE && obj->subtype != NULL && !strcmp(obj->subtype, "MCDRAM"))
    nc = snprintf(info_in, n, "%s", obj->subtype);
  else nc = hwloc_obj_type_snprintf(info_in, n, obj, 0);
  if((int)obj->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE)){
    nc += snprintf(info_in+nc,n-nc,":%d ",obj->logical_index);
  }
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

static hwloc_obj_t roofline_hwloc_local_memory(){
  hwloc_obj_t thread_location;
  hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
    unsigned node_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE);
  
  if(hwloc_get_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD) == -1){
    perror("get_cpubind");
    goto machine_bind_error;
  }
  if(hwloc_get_largest_objs_inside_cpuset(topology, cpuset, &thread_location, 1) == -1){
    perror("hwloc_get_largest_objs_inside_cpuset");
    goto machine_bind_error;
  }
  
  while(thread_location != NULL && thread_location->depth > node_depth) thread_location = thread_location->parent;
  hwloc_bitmap_free(cpuset);
  return thread_location;
  
machine_bind_error:
  hwloc_bitmap_free(cpuset);
  return NULL;
}

#ifdef DEBUG2
static hwloc_obj_t roofline_hwloc_get_largest_memory_inside_nodeset(const hwloc_bitmap_t nodeset){
  hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_NUMANODE, 0);
  while(obj != NULL && !hwloc_bitmap_isincluded(obj->nodeset, nodeset)) obj = obj->next_cousin;
  if(obj == NULL){roofline_debug2("No memory inside given nodeset\n"); return NULL;}
  while(obj->parent != NULL && hwloc_bitmap_isincluded(obj->parent->nodeset, nodeset)) obj = obj->parent;
  return obj;
}

static void roofline_hwloc_print_area_membind(const hwloc_obj_t membind_location, const void * ptr, const size_t size){
  hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();
  hwloc_membind_policy_t policy;

  if(hwloc_get_area_membind(topology, ptr, size, nodeset, &policy, HWLOC_MEMBIND_BYNODESET) == -1){
    perror("hwloc_get_area_membind");
    goto print_area_error;
  }
  hwloc_obj_t location = roofline_hwloc_get_largest_memory_inside_nodeset(nodeset);
  if(location != NULL){
    roofline_mkstr(area_str,128);
    roofline_mkstr(target,128);
    roofline_hwloc_obj_snprintf(membind_location, target, sizeof(target));
    if(location != NULL) roofline_hwloc_obj_snprintf(location, area_str, sizeof(area_str));
    printf("area allocation on %s, allocated on %s\n", target, area_str);
  }

print_area_error:
  hwloc_bitmap_free(nodeset);
  return;
}
#endif

int roofline_hwloc_set_area_membind(const hwloc_obj_t membind_location, void * ptr, const size_t size){
  hwloc_obj_t where = membind_location;
  unsigned n_nodes = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_NUMANODE);
  unsigned node_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE);
  unsigned tid = 0, nid = 0;
  hwloc_membind_policy_t policy = HWLOC_MEMBIND_BIND;
  
  /* Check if binding is even necessary */
  if(n_nodes <= 1){goto area_membind_success;}
  if(where == NULL){return 0;}
  
  /* Bind on node */
  if(where != NULL && where->depth <= node_depth)
  {
    n_nodes = hwloc_get_nbobjs_inside_cpuset_by_type(topology, where->cpuset, HWLOC_OBJ_NUMANODE);
#ifdef _OPENMP
    tid = omp_get_thread_num();
#endif
    nid = tid*n_nodes/n_threads;
    
    /* If there are several NUMA memory possible, we have to choose among them where to bind memory. */
    if(n_nodes>1){
      /* Balance with interleave on remote nodes. More efficient than balancing as for local nodes */
      if(hwloc_bitmap_isequal(where->cpuset, root->cpuset)){ policy = HWLOC_MEMBIND_INTERLEAVE; }
      /* Balance round robin per group of threads (NUMA:0 <- group:0, NUMA:1 <- group:1) */
      else{where = hwloc_get_obj_inside_cpuset_by_type(topology, where->cpuset, HWLOC_OBJ_NUMANODE, nid);}
    }
  }
  /* Bind close to thread */
  else { where = roofline_hwloc_local_memory(); }

  /* Apply memory binding */
  if(where == NULL){return 0;}  
  if(hwloc_set_area_membind(topology, ptr, size, where->nodeset,policy,
			    HWLOC_MEMBIND_THREAD   |
			    HWLOC_MEMBIND_NOCPUBIND|
			    HWLOC_MEMBIND_STRICT   |
			    HWLOC_MEMBIND_MIGRATE  |
			    HWLOC_MEMBIND_BYNODESET) == -1){
    perror("hwloc_set_area_membind");
    return 0;
  }

  /* Print area location */
#ifdef DEBUG2
  roofline_hwloc_print_area_membind(membind_location, ptr, size);
#endif

area_membind_success:
  memset(ptr, 0, size);
  return 1;
}


inline int roofline_hwloc_objtype_is_cache(const hwloc_obj_type_t type){
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

int roofline_hwloc_cpubind(const hwloc_obj_type_t leaf_type){
  unsigned tid = 0;
#ifdef _OPENMP
  tid = omp_get_thread_num();
#endif
  unsigned n_leaves = hwloc_get_nbobjs_inside_cpuset_by_type(topology, root->cpuset, leaf_type);
  if(n_leaves == 0){
    roofline_mkstr(root_str, 32);
    roofline_hwloc_obj_snprintf(root, root_str, sizeof(root_str));
    fprintf(stderr,"cpubind failed because root %s has no child %s\n", root_str, hwloc_type_name(leaf_type));
    return 0;
  }
  hwloc_obj_t leaf = hwloc_get_obj_inside_cpuset_by_type(topology, root->cpuset, leaf_type, tid%n_leaves);
  if(leaf_type == HWLOC_OBJ_CORE) leaf = leaf->first_child;
  
  if(hwloc_set_cpubind(topology, leaf->cpuset, HWLOC_CPUBIND_THREAD|HWLOC_CPUBIND_STRICT|HWLOC_CPUBIND_NOMEMBIND) == -1){
    perror("cpubind");
    return 0;
  }
  
  return roofline_hwloc_check_cpubind(leaf->cpuset);
}

int roofline_hwloc_obj_is_memory(const hwloc_obj_t obj){
  hwloc_obj_cache_type_t type;
  
  if ((int)obj->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE)) return 1;
  if(roofline_hwloc_objtype_is_cache(obj->type)){
    type = obj->attr->cache.type;
    if(type == HWLOC_OBJ_CACHE_UNIFIED || type == HWLOC_OBJ_CACHE_DATA){
      return 1;
    }
  }
  return 0;
}

size_t roofline_hwloc_get_memory_size(const hwloc_obj_t obj){
  hwloc_obj_cache_type_t type;
  if(obj==NULL)
    return 0;
  if ((int)obj->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE))
    return obj->memory.total_memory;
  if(roofline_hwloc_objtype_is_cache(obj->type)){
    type = obj->attr->cache.type;
    if(type == HWLOC_OBJ_CACHE_UNIFIED || type == HWLOC_OBJ_CACHE_DATA){
      return ((struct hwloc_cache_attr_s *)obj->attr)->size;
    }
  }
  return 0;
}


hwloc_obj_t roofline_hwloc_get_under_memory(const hwloc_obj_t obj){
  hwloc_obj_t child = obj;
  if(obj==NULL) return NULL;
  unsigned node_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE);
  
  /* Handle memory without child case */
  while(child != NULL && child ->arity == 0){ child = child->prev_sibling; }
  if(child == NULL){return NULL;}
  
  /* go down to the first memory */
  do{ child = child->first_child; } while(child != NULL && (!roofline_hwloc_obj_is_memory(child) || child->depth <= node_depth));
  return child;
}


hwloc_obj_t roofline_hwloc_get_next_memory(const hwloc_obj_t obj){
  hwloc_obj_t next = obj;
  /* If current_obj is not set, start from the bottom of the topology to return the first memory */
  if(obj == NULL) next = hwloc_get_obj_inside_cpuset_by_depth(topology, root->cpuset, hwloc_topology_get_depth(topology)-1,0);
  
  /* If current obj is not a cache, then next memory is at same depth in root cpuset (if any) */
  if((int)next->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE) && next->next_cousin != NULL) return next->next_cousin;
  
  /* get parent memory */
  do{next=hwloc_get_obj_inside_cpuset_by_depth(topology, root->cpuset, next->depth-1, 0);} while(next!=NULL && !roofline_hwloc_obj_is_memory(next));
  /* If obj is a above NUMANode, then take the topology left most obj */
  if(next != NULL && (int)next->depth <= hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE))
    next = hwloc_get_obj_by_depth(topology, next->depth, 0);
  
  /* No memory left in topology */
  return next;
}

