#ifndef SAMPLING_H
#define SAMPLING_H
#include <stdint.h>

struct roofline_sample{
    /* All sample type specific data */
    uint64_t ts_start;       /* Timestamp in cycles where the roofline started */
    uint64_t ts_end;         /* Timestamp in cycles where the roofline ended */
    uint64_t flop_ins;       /* The number of flop instructions */
    uint64_t load_ins;       /* The number of load instructions */
    uint64_t store_ins;      /* The number of store instructions */
    uint64_t flops;          /* The amount of flops computed */
    uint64_t load_bytes;     /* The amount of load bytes transfered */
    uint64_t store_bytes;    /* The amount of store bytes transfered */
};

void roofline_sample_print(struct roofline_sample* , const char * info);
void roofline_sampling_init(const char * output);
void roofline_sampling_fini();
void roofline_eventset_init(int * eventset);
void roofline_eventset_destroy(int * eventset);
void roofline_sampling_start(int eventset, struct roofline_sample *);
void roofline_sampling_stop(int eventset, struct roofline_sample *);

#ifdef _OPENMP
#include <omp.h>
extern struct roofline_sample shared;
extern unsigned n_threads;

void roofline_sample_clear(struct roofline_sample *);
void roofline_sample_accumulate(struct roofline_sample *, struct roofline_sample *);

#define roofline_sampling_parallel_start() do{		\
	int                    eventset = PAPI_NULL;	\
	struct roofline_sample out;			\
	_Pragma("omp single")				\
	    roofline_sample_clear(&shared);		\
	_Pragma("omp critical")				\
	    roofline_eventset_init(&eventset);		\
	_Pragma("omp barrier")				\
	    roofline_sampling_start(eventset, &out)

#define roofline_sampling_parallel_stop(info)	\
    roofline_sampling_stop(eventset, &out);	\
    _Pragma("omp barrier")			\
    _Pragma("omp critical")			\
    roofline_sample_accumulate(&shared, &out);	\
    roofline_eventset_destroy(&eventset);	\
    n_threads = omp_get_num_threads();		\
    _Pragma("omp single")			\
    roofline_sampling_print(&shared, info);	\
    } while(0)

#endif /* _OPENMP */
#endif /*  SAMPLING_H */
