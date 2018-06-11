#ifndef TOPOLOGY_H
#define TOPOLOGY_H

#include <hwloc.h>

/* Allocation policies */
typedef enum{LARM_FIRSTTOUCH, LARM_INTERLEAVE, LARM_FIRSTTOUCH_HBM, LARM_INTERLEAVE_DDR, LARM_INTERLEAVE_HBM} LARM_policy;

extern hwloc_topology_t topology;   /* Current machine topology */
extern hwloc_obj_t      root;       /* The subroot of topology to select the amount of threads */
extern size_t           max_size;   /* The 32 * last level cache size */

void        roofline_hwloc_check_version();
int         roofline_hwloc_objtype_is_cache(const hwloc_obj_type_t type);
int         roofline_hwloc_obj_snprintf(const hwloc_obj_t obj, char * info_in, const size_t n);
hwloc_obj_t roofline_hwloc_parse_obj(const char*);

int         roofline_hwloc_memory_group_depth();
unsigned    roofline_hwloc_n_memory_group();
hwloc_obj_t roofline_hwloc_memory_group(unsigned i);
hwloc_obj_t roofline_hwloc_memory_group_inside_cpuset(unsigned i);
hwloc_obj_t roofline_hwloc_LLC(unsigned i);

/* CPU binding */
void        roofline_hwloc_cpubind(const hwloc_obj_t obj);
hwloc_obj_t roofline_hwloc_get_cpubind();
int         roofline_hwloc_check_cpubind(hwloc_cpuset_t cpuset);

/* Memory binding */
hwloc_obj_t   roofline_hwloc_set_area_membind(const hwloc_obj_t, void *, const size_t, LARM_policy);
hwloc_obj_t   roofline_hwloc_local_domain();
int           roofline_hwloc_iscache(const hwloc_obj_t obj);
int           roofline_hwloc_ismemory(const hwloc_obj_t);
unsigned long roofline_hwloc_memory_size(const hwloc_obj_t obj);
hwloc_obj_t   roofline_hwloc_get_next_memory(const hwloc_obj_t);
hwloc_obj_t   roofline_hwloc_get_under_memory(const hwloc_obj_t);
int           roofline_hwloc_get_memory_bounds(const hwloc_obj_t memory, size_t * lower, size_t * upper, const int op_type);

/* Merging cpusets */
void roofline_hwloc_accumulate(hwloc_obj_t * dst, hwloc_obj_t * src);

#endif /* TOPOLOGY_H */

