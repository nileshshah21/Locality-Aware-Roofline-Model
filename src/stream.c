#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "utils.h"
#include "stream.h"
#include "topology.h"

extern size_t alignement; /* Level 1 cache line size */

static size_t roofline_memalign(double ** data, size_t size){
    int err;
    if(data != NULL){
	err = posix_memalign((void**)data, alignement, size);
	switch(err){
	case 0:
	    break;
	case EINVAL:
	    fprintf(stderr,"The alignment argument was not a power of two, or was not a multiple of sizeof(void *).\n");
	    break;
	case ENOMEM:
	    fprintf(stderr,"There was insufficient memory to fulfill the allocation request.\n");
	}
	if(*data == NULL)
	    fprintf(stderr,"Chunk is NULL\n");
	if(err || *data == NULL)
	  ERR_EXIT("Roofline memalign unhandled error\n");
    }
    return size;
}

size_t get_chunk_size(int type);/* Minimum chunk size in MSC.h */

static size_t resize_splitable_chunk(size_t size, int type){
  size_t ret, chunk_size = get_chunk_size(type);
  if(size%chunk_size == 0) return size;
  else{
    ret = chunk_size*(size/chunk_size);
    return ret;
  }
}

roofline_stream new_roofline_stream(const size_t size, const int op_type){
  roofline_stream ret;  
  roofline_alloc(ret, sizeof(*ret));  
  roofline_memalign(&(ret->stream), size);
  ret->alloc_size = size;
  ret->size = resize_splitable_chunk(size, op_type);
  return ret;
}

void roofline_stream_split(roofline_stream in, roofline_stream chunk, unsigned n_chunk, unsigned chunk_id, int op_type)
{
  chunk->alloc_size = in->alloc_size/n_chunk;
  chunk->size = resize_splitable_chunk(in->size/n_chunk, op_type);
  chunk->stream = in->stream + (chunk_id%n_chunk)*chunk->alloc_size/sizeof(*(in->stream));
  roofline_debug1("Split buffer(%luB) into %luB. Chunk is at %luB\n",
		  in->alloc_size, chunk->alloc_size, (chunk_id%n_chunk)*chunk->alloc_size);
}

void roofline_stream_set_size(roofline_stream in, const size_t size, const int op_type){
  if(size>in->alloc_size){
    free(in->stream);
    roofline_memalign(&(in->stream), size);
    in->alloc_size = size;
  }
  in->size = resize_splitable_chunk(size, op_type);
}

void delete_roofline_stream(roofline_stream in){
  free(in->stream);
  free(in);
}

