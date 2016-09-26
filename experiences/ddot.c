#include <cblas.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _OPENMP
#include <omp.h>
#endif

double ddot(const int N, const double *X, const double *Y){
  int i;
  double sum = 0;
#ifdef _OPENMP
#pragma omp parallel for reduction(+:sum)
#endif
  for(i=0; i<N; i++){sum += X[i] * Y[i];}
  return sum;
}

double blas_ddot(const int N, const double *X, const double *Y){
  return cblas_ddot(N, X, 1, Y, 1);
}

double avx_ddot(int N, double *X, double *Y){
  volatile int i, r;
  volatile double result = 0;
  volatile double * x = X, *y = Y; 
#ifdef _OPENMP
#pragma omp parallel firstprivate(N,X,Y) reduction (+:result)
  {
    int tid = omp_get_thread_num();
    int nt = omp_get_num_threads(); 
    r = N%nt;
    N = N/nt;
    x = &(X[N*tid]);
    y = &(Y[N*tid]);
    if(nt-1 == tid) N+=r;
#endif
    r = N%16;
    N = N-r;
    double *sum = malloc(sizeof(double)*4);

    __asm__ __volatile__ (						\
      "vpxor %%ymm0, %%ymm0, %%ymm0\n\t"				\
      "loop_ddot:\n\t"							\
      "vmovupd (%1), %%ymm1\n\t"					\
      "vmovupd (%2), %%ymm2\n\t"					\
      "vfmadd231pd %%ymm2, %%ymm1, %%ymm0\n\t"                          \
      "vmovupd 32(%1), %%ymm3\n\t"					\
      "vmovupd 32(%2), %%ymm4\n\t"					\
      "vfmadd231pd %%ymm4, %%ymm3, %%ymm0\n\t"                          \
      "vmovupd 64(%1), %%ymm5\n\t"					\
      "vmovupd 64(%2), %%ymm6\n\t"					\
      "vfmadd231pd %%ymm6, %%ymm5, %%ymm0\n\t"                          \
      "vmovupd 96(%1), %%ymm7\n\t"					\
      "vmovupd 96(%2), %%ymm8\n\t"					\
      "vfmadd231pd %%ymm8, %%ymm7, %%ymm0\n\t"                          \
      "add $128, %1\n\t"						\
      "add $128, %2\n\t"						\
      "sub $16, %3\n\t"							\
      "jnz loop_ddot\n\t"						\
      "vmovupd %%ymm0, (%0)\n\t"					\
      ::"r" (sum), "r" (x), "r" (y), "r" (N)				\
      : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7", "%xmm8", "memory");
    
    result = sum[0] + sum[1] + sum[2] + sum[3];
    for(i = N; i<N+r; i++){result += x[i]*y[i];}

    free(sum);
#ifdef _OPENMP
  }
#endif

  return result;
}

#define pprintf(...) _Pragma("omp single") printf(__VA_ARGS__)

void circular_ddot(const int n_iter, size_t buf_size){
  const double beta = (double)rand()/(double)RAND_MAX;
  const double alpha = (double)rand()/(double)RAND_MAX;
  int N = buf_size/(2*sizeof(double));

  /* Global distributed vector split */
  double ** X;
  /* The matrix is stored distributed */
  
#pragma omp parallel firstprivate(N)
  {
    int n_threads = omp_get_num_threads();
    int i, n, tid = omp_get_thread_num();
    double *y, *x, local_ddot = 0;

#pragma omp master
    X = malloc(sizeof(double*)*n_threads);
#pragma omp barrier
    
    pprintf("vectors initialization...\n");
      
    /* split work among threads */
    n = N/n_threads;
    if((tid+1)%n_threads==0) n += N%n_threads;
    N = N/n_threads;
    
    /* Allocate vectors */
    y = malloc(n*sizeof(double));
    x = malloc(n*sizeof(double));
    
    /* fill matrix and vector */
    for(i=0;i<n;i++) x[i] = (double)rand()/(double)RAND_MAX;
    for(i=0;i<n;i++) y[i] = (double)rand()/(double)RAND_MAX;

    X[tid] = x;
#pragma omp barrier
    pprintf("vectors initialization done\n");
    
    for(i=0; i < n_iter*n_threads; i++){
      /* compute dgemv */
      local_ddot += blas_ddot(n, x, y);
      
      /* Synchronize with other threads then exchange vectors */
      X[(tid+1)%n_threads] = x;
#pragma omp barrier
      x = X[tid];
      pprintf("Iteration %d/%d done\n", i+1, n_iter*n_threads);
    }
    
    /* Cleanup */
    free(y);
    free(x);
  }
  free(X);
}

