#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include <hwloc.h>

void        roofline_hwloc_check_version();
int         roofline_hwloc_objtype_is_cache(hwloc_obj_type_t type);
int         roofline_hwloc_obj_snprintf(hwloc_obj_t obj, char * info_in, size_t n);
hwloc_obj_t roofline_hwloc_parse_obj(char*);
int         roofline_hwloc_cpubind(hwloc_obj_t);
int         roofline_hwloc_membind(hwloc_obj_t);
int         roofline_hwloc_obj_is_memory(hwloc_obj_t);
size_t      roofline_hwloc_get_memory_size(hwloc_obj_t);
hwloc_obj_t roofline_hwloc_get_next_memory(hwloc_obj_t, int vertical);
hwloc_obj_t roofline_hwloc_get_under_memory(hwloc_obj_t, int whole_system);
int         roofline_hwloc_get_memory_bounds(hwloc_obj_t memory, size_t * lower, size_t * upper, int op_type);

#endif /* TOPOLOGY_H */
