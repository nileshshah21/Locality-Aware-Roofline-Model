#ifndef TYPES_H
#define TYPES_H

#include <hwloc.h>

#define ROOFLINE_LOAD     1
#define ROOFLINE_LOAD_NT  2
#define ROOFLINE_STORE    4
#define ROOFLINE_STORE_NT 8
#define ROOFLINE_2LD1ST   16
#define ROOFLINE_COPY     32
#define ROOFLINE_MUL      64
#define ROOFLINE_ADD      128
#define ROOFLINE_MAD      256
#define ROOFLINE_FMA      512

int          roofline_type_from_str(const char * type);
const char * roofline_type_str(int type);
int          roofline_filter_types(hwloc_obj_t obj, int type);

#endif /* TYPES_H */

