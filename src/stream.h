#ifndef STREAM_H
#define STREAM_H

#include <stdlib.h>

typedef double * roofline_stream_t;
typedef struct roofline_stream_s{
  roofline_stream_t stream;     /* The buffer for stream */
  size_t            alloc_size; /* Size of stream */
  size_t            size;       /* Actually usable part of the stream */
} * roofline_stream;

roofline_stream new_roofline_stream(const size_t size, const int op_type);
void            delete_roofline_stream(roofline_stream);
void            roofline_stream_split(const roofline_stream in,
				      roofline_stream chunk,
				      const unsigned n_chunk,
				      const unsigned chunk_id,
				      const int op_type);
void            roofline_stream_set_size(roofline_stream in, const size_t size, const int op_type);

#endif /* STREAM_H */
