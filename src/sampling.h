#ifndef SAMPLING_H
#define SAMPLING_H

#define TYPE_LOAD  1
#define TYPE_STORE 2

struct roofline_sample;

void                     roofline_sampling_init (const char * output);
void                     roofline_sampling_fini ();
struct roofline_sample * new_roofline_sample    (int type);
void                     roofline_sample_set(struct roofline_sample*, int type, long flops, long bytes);
void                     roofline_sample_clear  (struct roofline_sample *);
void                     roofline_sampling_start(struct roofline_sample *);
void                     roofline_sampling_stop (struct roofline_sample *);
void                     roofline_sample_print  (struct roofline_sample* , const char * info);
void                     delete_roofline_sample (struct roofline_sample *);

#ifdef _OPENMP
#include <omp.h>
extern struct roofline_sample shared;
extern unsigned n_threads;

void roofline_sample_accumulate(struct roofline_sample *, struct roofline_sample *);

#define roofline_sampling_parallel_start() do{			\
    struct roofline_sample * out = new_roofline_sample();	\
    _Pragma("omp single")					\
      roofline_sample_clear(&shared);				\
    _Pragma("omp barrier")					\
      roofline_sampling_start(out)

#define roofline_sampling_parallel_stop(info)	\
  roofline_sampling_stop(out);			\
  _Pragma("omp barrier")			\
  _Pragma("omp critical")			\
  roofline_sample_accumulate(&shared, &out);	\
  delete_roofline_sample(out);			\
  n_threads = omp_get_num_threads();		\
  _Pragma("omp single")				\
    roofline_sampling_print(&shared, info);	\
} while(0)

#endif /* _OPENMP */
#endif /*  SAMPLING_H */
