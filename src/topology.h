#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include <hwloc.h>

extern hwloc_topology_t topology;   /* Current machine topology */
extern hwloc_obj_t      root;       /* The subroot of topology to select the amount of threads */
extern size_t           max_size;   /* The 32 * last level cache size */

void        roofline_hwloc_check_version();
int         roofline_hwloc_objtype_is_cache(hwloc_obj_type_t type);
int         roofline_hwloc_obj_snprintf(hwloc_obj_t obj, char * info_in, size_t n);
hwloc_obj_t roofline_hwloc_parse_obj(const char*);
int         roofline_hwloc_cpubind(hwloc_obj_type_t);
int         roofline_hwloc_set_area_membind(hwloc_obj_t, void *, size_t);
int         roofline_hwloc_obj_is_memory(hwloc_obj_t);
size_t      roofline_hwloc_get_memory_size(hwloc_obj_t);
hwloc_obj_t roofline_hwloc_get_next_memory(hwloc_obj_t);
hwloc_obj_t roofline_hwloc_get_under_memory(hwloc_obj_t);
int         roofline_hwloc_get_memory_bounds(hwloc_obj_t memory, size_t * lower, size_t * upper, int op_type);

#endif /* TOPOLOGY_H */
