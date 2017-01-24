#ifndef SAMPLING_H
#define SAMPLING_H

#define TYPE_LOAD  1
#define TYPE_STORE 2

/**
 * Initialize the sampling library.
 * This function internally initialize a topology where samples are stored, the PAPI library.
 * Each processing unit of the topology own a sample containing flops and bytes count and is updated 
 * by threads calling roofline_sampling_start() from this processing unit.
 * If the library is compiled with openmp enabled, it is thread safe, and thread synchronization occures on roofline_sampling_stop() call. 
 * @arg output: The file where to write the output on call to sampling stop.
 * @arg type: the hardware counters allow only a limited number of counter on each processing unit. Hence it is necessary to choos if bytes 
 *            will be counted for load operations or store operations. To count them both, it is necessary to restart the program with a new 
 *            initialization of the library.
 **/
void   roofline_sampling_init(const char * output, int type);

/**
 * Destroy structures initialized on call to roofline_sampling_init.
 **/
void   roofline_sampling_fini ();

/**
 * Initialize a sampling structure and start counting. In an openmp context, each thread will count for itself,
 * and result accumulation is handled automatically to be presented by NUMA domain on roofline_sampling_stop() call.
 * If the library is compiled with PAPI enabled, then flops and bytes are counted using hardware counters; Else the arguments of the function
 * call are used to accumulate counts into the NUMA domains.
 * roofline_sampling_stop() must be called from the same scope as roofline_sampling_start().
 * If the library is compiled with openmp enabled, then a call from a parallel region will become parallel also.
 * If the library is compiled with openmp enabled and flag parallel is True, then a call from a sequential region will be parallel and thread 
 * sampling will be made with current thread affinity. Be aware that the parallel call add a great overhead to byte counter.
 * If the library is compiled with openmp enabled and flag parallel is False, then a call from a sequential region will be sequential.
 * @arg force_parallel: a flag to tell if we have to count in parallel even if called from a sequential region
 * @arg flops: by hand flop count.
 * @arg bytes: by hand byte count.
 * @return a pointer to an opaque struct holding the current sample. 
 *         This sample pointer must be provided to roofline_sampling_stop().
 **/
void * roofline_sampling_start(int force_parallel, long flops, long bytes);

/**
 * Stop sampling roofline metrics and output results for the whole machine. If compiled with openmp support, a barrier occures during this call.
 * Result presentation is as follow:
 * NUMA_domain Nanoseconds Bytes Flops n_threads type info.
 * Results are accumulated by NUMA domain.
 * Nanoseconds is the time between roofline_sampling_start() and roofline_sampling_stop() calls.
 * Bytes and Flops are counted between roofline_sampling_start() and roofline_sampling_stop() calls, whether with hardware counters or with
 * manually provided values.
 * n_threads is the number of processing units where something was actually counted.
 * type is whether load or store depending on the library initialization type.
 * Additionnally, the environment variable "LARM_INFO" is read on each roofline_sampling_stop() call, and is appended to the info column of the
 * sample result.
 * @arg sample: The structure provided on roofline_sampling_start() call.
 * @arg info: an additional information to be written into info column.
 **/
void   roofline_sampling_stop (void * sample, const char* info);

#endif /*  SAMPLING_H */
