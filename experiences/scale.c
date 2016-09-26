#include <stdlib.h>
#ifdef _OPENMP
#include <omp.h>
#endif

void scale(const int N, const double * X, double * Y, const double scalar){
  int i;
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(i=0; i<N; i++) {Y[i] = scalar*X[i];}
}

void avx_scale(int N, double *X, double *Y, double scalar){
  volatile int i, r;
  volatile double * x = X, *y = Y; 
  double *vscale = malloc(sizeof(double)*4);
  vscale[0] = vscale[1] = vscale[2] = vscale[3] = scalar;

#ifdef _OPENMP
#pragma omp parallel firstprivate(N,x,y)
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
    __asm__ __volatile__ (						\
      "vmovapd (%0), %%ymm0\n\t"					\
      "loop_scale:\n\t"							\
      "vmovupd (%1), %%ymm1\n\t"					\
      "vmulpd %%ymm0, %%ymm1, %%ymm2\n\t"				\
      "vmovupd %%ymm2, (%2)\n\t"					\
      "vmovupd 32(%1), %%ymm3\n\t"					\
      "vmulpd %%ymm0, %%ymm3, %%ymm4\n\t"				\
      "vmovupd %%ymm4, 32(%2)\n\t"					\
      "vmovupd 64(%1), %%ymm5\n\t"					\
      "vmulpd %%ymm0, %%ymm5, %%ymm6\n\t"				\
      "vmovupd %%ymm6, 64(%2)\n\t"					\
      "vmovupd 96(%1), %%ymm7\n\t"					\
      "vmulpd %%ymm0, %%ymm7, %%ymm8\n\t"				\
      "vmovupd %%ymm7, 96(%2)\n\t"					\
      "add $128, %1\n\t"						\
      "add $128, %2\n\t"						\
      "sub $16, %3\n\t"							\
      "jnz loop_scale\n\t"						\
      ::"r" (vscale), "r" (x), "r" (y), "r" (N)				\
      : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7", "%xmm8", "memory");
    for(i = N; i<N+r; i++){y[i] = scalar*x[i];}

#ifdef _OPENMP
  }
#endif
  free(vscale);
}

