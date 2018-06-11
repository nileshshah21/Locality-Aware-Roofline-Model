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
  hwloc_obj_t LLC            = roofline_hwloc_LLC(0);
  hwloc_obj_t lower_memory   = memory;
  unsigned    n_lower_memory = 1;
  unsigned    n_lower_cpu    = 1;
  unsigned    n_upper_cpu    = 1;  
  size_t      lower_size     = 0;
  size_t      upper_size     = roofline_hwloc_memory_size(memory);
  
  /* Set lower bound size according to child caches */
  lower_memory  = roofline_hwloc_get_under_memory(memory);

  /* Memory is L1DCACHE */
  if(lower_memory == NULL){
    n_lower_memory = 1;
    n_lower_cpu    = 1;
    lower_size     = get_chunk_size(op_type)*4;
  } else if(roofline_hwloc_iscache(memory)){
    n_lower_memory = hwloc_get_nbobjs_inside_cpuset_by_type(topology, memory->cpuset, lower_memory->type);
    n_lower_cpu    = hwloc_get_nbobjs_inside_cpuset_by_type(topology, lower_memory->cpuset, HWLOC_OBJ_CORE);
    n_upper_cpu    = hwloc_get_nbobjs_inside_cpuset_by_type(topology, memory->cpuset, HWLOC_OBJ_CORE);
    lower_size     = roofline_hwloc_memory_size(lower_memory);    
  } else {
    lower_memory   = LLC;
    n_lower_memory = hwloc_get_nbobjs_by_type(topology, lower_memory->type);    
    n_lower_cpu    = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
    n_upper_cpu    = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
    lower_size     = roofline_hwloc_memory_size(lower_memory);
  }
  
  /* Set bounds size */
  *lower = n_lower_memory * lower_size / n_upper_cpu;
  *upper = upper_size / n_upper_cpu;
     
  /* Upper bound size is greater than 1GB (slow). Try to reduce size */  
  while(*upper > GB && *upper > 2*(*lower)) { *upper = *upper/2; }
  
#ifdef DEBUG2
  roofline_mkstr(target, 128);
  roofline_hwloc_obj_snprintf(memory, target, sizeof(target));
  printf("Memory bounds for %s set at: [%ldB, %ldB]\n", target, *lower, *upper);
#endif
    
  return 0;
}

int roofline_hwloc_obj_snprintf(const hwloc_obj_t obj, char * info_in, const size_t n){
  int nc;
  memset(info_in,0,n);
  /* Special case for MCDRAM */
  if(obj->type == HWLOC_OBJ_NUMANODE && obj->subtype != NULL && !strcmp(obj->subtype, "MCDRAM"))
    nc = snprintf(info_in, n, "%s", obj->subtype);
  else nc = hwloc_obj_type_snprintf(info_in, n, obj, 0);
  nc += snprintf(info_in+nc,n-nc,":%d ",obj->logical_index);
  return nc;
}

hwloc_obj_t roofline_hwloc_get_cpubind(){
  hwloc_obj_t    bound;
  hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();
  if(hwloc_get_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD) == -1){
    perror("get_cpubind");
    hwloc_bitmap_free(cpuset);
    return NULL; 
  }
  
  bound = hwloc_get_first_largest_obj_inside_cpuset(topology, cpuset);
  free(cpuset);
  return bound;
}

void roofline_hwloc_cpubind(const hwloc_obj_t obj){
  hwloc_obj_t leaf = hwloc_get_obj_inside_cpuset_by_type(topology, obj->cpuset, HWLOC_OBJ_PU, 0);
  if(hwloc_set_cpubind(topology, leaf->cpuset, HWLOC_CPUBIND_THREAD|HWLOC_CPUBIND_STRICT|HWLOC_CPUBIND_NOMEMBIND) == -1){
    perror("cpubind");
    return;
  }
  if(!roofline_hwloc_check_cpubind(leaf->cpuset)){return;}
  return;
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
    hwloc_obj_t node = roofline_hwloc_local_domain();
    fprintf(stderr, "cpubind=%s:%d, bound %s:%d, node %s:%d\n",
	   hwloc_obj_type_string(bind->type),bind->logical_index,
	    hwloc_obj_type_string(bound->type),bound->logical_index,
	    hwloc_obj_type_string(node->type),node->logical_index);
#endif /* DEBUG2 */
  ret = hwloc_bitmap_isequal(cpuset,checkset);
  hwloc_bitmap_free(checkset);  
  return ret;
}

hwloc_obj_t roofline_hwloc_local_domain(){
  int groupd = roofline_hwloc_memory_group_depth();
  
  hwloc_obj_t location = roofline_hwloc_get_cpubind();
  if(location == NULL){
      fprintf(stderr, "Cannot find binding of current thread\n");
      return NULL;
  }
  if(location->depth <= groupd){return location;}
  while(location->depth > groupd){ location = location->parent; }
  if(location == NULL){
      fprintf(stderr, "Thread has no parent Group\n");
      return NULL;
  }
  return location;
}

static hwloc_obj_t roofline_hwloc_get_largest_memory_inside_nodeset(const hwloc_bitmap_t nodeset){
  hwloc_obj_t obj = roofline_hwloc_memory_group(0);
  while(obj != NULL && !hwloc_bitmap_isincluded(obj->nodeset, nodeset)) obj = obj->next_cousin;
  if(obj == NULL){roofline_debug2("No memory inside given nodeset\n"); return NULL;}
  while(obj->parent != NULL && hwloc_bitmap_isincluded(obj->parent->nodeset, nodeset)) obj = obj->parent;
  return obj;
}

static hwloc_obj_t roofline_hwloc_get_area_membind(const void * ptr, const size_t size){
  hwloc_bitmap_t nodeset = hwloc_bitmap_alloc();
  hwloc_membind_policy_t policy;

  if(hwloc_get_area_membind(topology, ptr, size, nodeset, &policy, HWLOC_MEMBIND_BYNODESET) == -1){
    perror("hwloc_get_area_membind");
    goto print_area_error;    
  }
  
  hwloc_obj_t location = roofline_hwloc_get_largest_memory_inside_nodeset(nodeset);
  print_area_error:
  hwloc_bitmap_free(nodeset);
  return location;
}

static void roofline_check_area_membind(const hwloc_obj_t target_l, const hwloc_obj_t alloc_l){
  roofline_mkstr(area,128); roofline_mkstr(target,128); roofline_mkstr(source,128);
  roofline_hwloc_obj_snprintf(target_l, target, sizeof(target));
  roofline_hwloc_obj_snprintf(alloc_l, area, sizeof(area));
  snprintf(source, sizeof(source), "unknown location");    
  hwloc_obj_t from = roofline_hwloc_local_domain();
  if(from != NULL){ memset(source, 0, sizeof(source)); roofline_hwloc_obj_snprintf(from, source, sizeof(source)); }    
  printf("area allocation on %s, allocated on %s from %s\n", target, area, source);
}

hwloc_obj_t roofline_hwloc_set_area_membind(const hwloc_obj_t membind_location, void * ptr, const size_t size, LARM_policy policy){
  hwloc_obj_t            where        = roofline_hwloc_local_domain();  
  hwloc_membind_policy_t hwloc_policy = HWLOC_MEMBIND_BIND;
  int                    NUMA_domain_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_NUMANODE);  
  hwloc_bitmap_t         nodeset      = hwloc_bitmap_alloc();
  
  if(where == NULL || membind_location == NULL || ptr == NULL){goto exit_alloc;}
  /* If their is more than one node where to bind, we have to apply user policy instead of force binding on a single node */
  if(membind_location->depth<NUMA_domain_depth){
    switch(policy){
    case LARM_FIRSTTOUCH:
      where = where->memory_first_child;
      hwloc_bitmap_copy(nodeset, where->nodeset);
      break;
    case LARM_INTERLEAVE:
      hwloc_policy = HWLOC_MEMBIND_INTERLEAVE;
      where = membind_location;
      hwloc_bitmap_copy(nodeset, where->nodeset);      
      break;
    case LARM_FIRSTTOUCH_HBM:
      where = where->memory_first_child;
      do{ where = where->next_sibling; } while(where && (where->subtype == NULL || strcmp(where->subtype, "MCDRAM")));
      if(where == NULL){
	fprintf(stderr, "No HBM found bind firsttouch DDR\n");
        where = roofline_hwloc_local_domain();
      }
      hwloc_bitmap_copy(nodeset, where->nodeset);
      break;
    case LARM_INTERLEAVE_DDR:
      hwloc_policy = HWLOC_MEMBIND_INTERLEAVE;
      where = membind_location;      
      while(where != NULL && where->depth<NUMA_domain_depth){where = where->first_child;}
      while(where != NULL && hwloc_bitmap_isincluded(where->nodeset, membind_location->nodeset)){
	if(where->subtype == NULL || strcmp(where->subtype, "MCDRAM")){
	  hwloc_bitmap_or(nodeset, where->nodeset, nodeset);
	}
	where = where->next_cousin;	  	
      }
      where = membind_location;
      break;
    case LARM_INTERLEAVE_HBM:
      hwloc_policy = HWLOC_MEMBIND_INTERLEAVE;
      where = membind_location;      
      while(where != NULL && where->depth<NUMA_domain_depth){where = where->first_child;}
      while(where != NULL && hwloc_bitmap_isincluded(where->nodeset, membind_location->nodeset)){	
	if(where->subtype != NULL && !strcmp(where->subtype, "MCDRAM")){
	  hwloc_bitmap_or(nodeset, where->nodeset, nodeset);
	}
	where = where->next_cousin;	
      }
      where = membind_location;
      break;
    }
  } else if(membind_location->depth>NUMA_domain_depth){
    where = roofline_hwloc_local_domain();
    hwloc_bitmap_copy(nodeset, where->nodeset);          
  } else {
    where = membind_location;
    hwloc_bitmap_copy(nodeset, where->nodeset);          
  }
  
  /* Apply memory binding */
  if(hwloc_set_area_membind(topology, ptr, size, nodeset, hwloc_policy,
			    HWLOC_MEMBIND_THREAD   |
			    HWLOC_MEMBIND_NOCPUBIND|
			    HWLOC_MEMBIND_STRICT   |
			    HWLOC_MEMBIND_MIGRATE  |
			    HWLOC_MEMBIND_BYNODESET) == -1){
    perror("hwloc_set_area_membind");
    return NULL;
  }
  
  memset(ptr, 0, size);
#ifdef DEBUG2
  roofline_check_area_membind(membind_location, roofline_hwloc_get_area_membind(ptr, size));
#endif

exit_alloc:
  hwloc_bitmap_free(nodeset);
  return where;
}


int roofline_hwloc_iscache(const hwloc_obj_t obj){
  return (obj->type==HWLOC_OBJ_L1CACHE ||
	  obj->type==HWLOC_OBJ_L2CACHE ||
	  obj->type==HWLOC_OBJ_L3CACHE ||
	  obj->type==HWLOC_OBJ_L4CACHE ||
	  obj->type==HWLOC_OBJ_L5CACHE);
}

int roofline_hwloc_ismemory(const hwloc_obj_t obj){
  return roofline_hwloc_iscache(obj) || obj->type == HWLOC_OBJ_NUMANODE;
}

unsigned long roofline_hwloc_memory_size(const hwloc_obj_t obj){
  if(obj == NULL){ return 0; }
  
  if(roofline_hwloc_iscache(obj)){
    return ((struct hwloc_cache_attr_s *)obj->attr)->size;
  }
  if(obj->type == HWLOC_OBJ_NUMANODE){
    return obj->total_memory;
  }

  fprintf(stderr, "Querying size of %s which is not a memory.\n", hwloc_obj_type_string(obj->type));
  return 0;
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
  hwloc_obj_t obj = hwloc_get_obj_by_depth(topology,depth,logical_index);
  //while(obj && obj->type==HWLOC_OBJ_NUMANODE){ obj = obj->parent; }
  return obj;

err_parse_obj:
  free(dup_arg);
  return NULL;
}

static hwloc_obj_t roofline_hwloc_first_memory_group(){
  hwloc_obj_t group = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, 0);
  while(group && group->memory_first_child == NULL){ group = group->parent; }
  return group;
}

int roofline_hwloc_memory_group_depth(){
  hwloc_obj_t group = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, 0);
  while(group && group->memory_first_child == NULL){ group = group->parent; }
  if(group != NULL){return group->depth;}
  else return 0;
}

unsigned roofline_hwloc_n_memory_group(){
  hwloc_obj_t group = roofline_hwloc_first_memory_group();
  if(group == NULL){return 0;}
  return hwloc_get_nbobjs_by_type(topology, group->type);
}

hwloc_obj_t roofline_hwloc_memory_group(unsigned i){
  hwloc_obj_t group = roofline_hwloc_first_memory_group();
  while(group && i--){ group = group->next_cousin; }
  return group;
}

hwloc_obj_t roofline_hwloc_memory_group_inside_cpuset(unsigned i){
  hwloc_obj_t group = roofline_hwloc_first_memory_group();
  while(group && i--){ group = group->next_cousin; }
  return group;
}

hwloc_obj_t roofline_hwloc_LLC(unsigned i){
  hwloc_obj_t LLC = hwloc_get_obj_by_type(topology, HWLOC_OBJ_MACHINE, 0);
  while(LLC && ! roofline_hwloc_iscache(LLC)){ LLC = LLC->first_child; }
  while(LLC && i--){ LLC = LLC->next_cousin; }
  return LLC;
}

hwloc_obj_t roofline_hwloc_get_under_memory(const hwloc_obj_t obj){
  hwloc_obj_t child   = obj;
  hwloc_obj_t LLC     = roofline_hwloc_LLC(0);
  int         L1d     = hwloc_get_type_depth(topology, HWLOC_OBJ_L1CACHE);
  int         groupd  = roofline_hwloc_memory_group_depth();
  
  if(obj==NULL) { return NULL; }

  // If obj has memory children, then they are for sure the memory to return.
  if(obj->memory_first_child != NULL){
    return obj->memory_first_child;
  }

  // If obj is a NUMANODE, then LLC is for sure the memory to return.
  if(obj->type == HWLOC_OBJ_NUMANODE){
    child = LLC;
    while(child                                                        &&
	  !hwloc_bitmap_isincluded(child->cpuset, obj->parent->cpuset) &&
	  !hwloc_bitmap_isincluded(obj->parent->cpuset, child->cpuset)){
      child = child->next_cousin;
    }
    return child;    
  }

  // If obj is a LLC, then the next memory to return is a cache.
  if(obj->type == LLC->type){
    child = obj->first_child;
    while(child && !roofline_hwloc_iscache(child)){
      child = child->first_child;
    }
    return child;
  }

  // else return the next object below which is a memory but not a cache.
  child = obj->first_child;
  while(child && !roofline_hwloc_ismemory(child)){
    child = child->first_child;      
  }
  return child;
}


hwloc_obj_t roofline_hwloc_get_next_memory(const hwloc_obj_t obj){
  hwloc_obj_t next    = NULL;
  int         L1d     = hwloc_get_type_depth(topology, HWLOC_OBJ_L1CACHE);
  int         groupd  = roofline_hwloc_memory_group_depth();
  
  /* If current_obj is not set, start from the bottom of the topology to return the first memory */
  if(obj == NULL || (obj->depth < 0 && obj->type != HWLOC_OBJ_NUMANODE) || obj->depth > L1d){
    return hwloc_get_obj_by_type(topology, HWLOC_OBJ_L1CACHE, 0);
  }

  if(obj->type == HWLOC_OBJ_NUMANODE){
    next = obj->next_cousin;
    if(next == NULL){      
      next = obj->parent;
      while(next && !roofline_hwloc_ismemory(next)){ next = next->parent; }
      return next;
    }
  } else {
    next = obj->parent;
    while(next && !roofline_hwloc_ismemory(next)){
      if(next->memory_first_child != NULL){
	return next->memory_first_child;
      }
      next = next->parent;
    }
    return next;
  }
}

void roofline_hwloc_accumulate(hwloc_obj_t * dst, hwloc_obj_t * src){
  /* merge cpusets */

  if( (dst == NULL || *dst == NULL) && (src == NULL || *src == NULL)){
    return;
  }
  if( (dst == NULL || *dst == NULL) ){
    *dst = *src;
    return;
  }
  if(dst != NULL && *dst == NULL && src != NULL && *src == NULL){
    while(*dst != NULL && (*dst)->parent != NULL && ! hwloc_bitmap_isincluded((*src)->cpuset,(*dst)->cpuset)){
      (*dst) = (*dst)->parent;
    }
    return;
  }
}

