#ifndef UTILS_H
#define UTILS_H

#include <malloc.h>
#include <stdio.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#define STR(x) #x
#define STRINGIFY(x) STR(x) 
#define CONCATENATE(X,Y) X ( Y )

#define ERR(...) do{					\
    fprintf(stderr, "%s:", __PRETTY_FUNCTION__);	\
    fprintf(stderr, __VA_ARGS__ );			\
  } while(0)

#define ERR_EXIT(...) do{						\
    ERR(__VA_ARGS__);							\
    exit(EXIT_FAILURE);							\
  } while(0);

#define PERR_EXIT(msg) do{perror(msg); exit(EXIT_FAILURE);} while(0);

#ifdef DEBUG2
#define roofline_debug2(...) fprintf(stderr, __VA_ARGS__)
#else
#define roofline_debug2(...)
#endif
#ifdef DEBUG1
#define roofline_debug1(...) fprintf(stderr, __VA_ARGS__)
#else
#define roofline_debug1(...)
#endif

#define roofline_MAX(x, y) ((x)>(y) ? (x):(y))
#define roofline_MIN(x, y) ((x)<(y) ? (x):(y))

#define roofline_alloc(ptr,size) do{if(!(ptr=malloc(size))){PERR_EXIT("malloc");}} while(0)

#ifdef _OPENMP

#define roofline_rdtsc(c_high,c_low)					\
  _Pragma("omp barrier")						\
  __asm__ __volatile__ (						\
    "CPUID\n\t"								\
    "RDTSC\n\t"								\
    "movq %%rdx, %0\n\t"						\
    "movq %%rax, %1\n\t" :"=r" (c_high), "=r" (c_low)::"%rax", "%rbx", "%rcx", "%rdx")

#else

#define roofline_rdtsc(c_high,c_low)		\
  __asm__ __volatile__ (\
    "CPUID\n\t"								\
    "RDTSC\n\t"								\
    "movq %%rdx, %0\n\t"						\
    "movq %%rax, %1\n\t" :"=r" (c_high), "=r" (c_low)::"%rax", "%rbx", "%rcx", "%rdx")
#endif
#define roofline_rdtsc_diff(high, low) ((high << 32) | low)



#endif /* UTILS_H */
