#include <stdlib.h>
#ifdef _OPENMP
#include <omp.h>
#endif

void triad(const int N, const double * A, const double * B, double * C, const double scalar){
  int i;
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(i=0; i<N; i++) {C[i] = A[i]+scalar*B[i];}
}

void avx_triad(int N, double * A, double * B, double * C, const double scalar){
  int r;
  double *vscale = malloc(sizeof(double)*4);
  vscale[0] = vscale[1] = vscale[2] = vscale[3] = scalar;

#ifdef _OPENMP
#pragma omp parallel firstprivate(N,A,B,C)
  {
    int tid = omp_get_thread_num();
    int nt = omp_get_num_threads(); 
    int r = N%nt;
    N = N/nt;
    A = &(A[N*tid]); B = &(B[N*tid]); C = &(C[N*tid]);
    if(nt-1 == tid) N+=r;
#endif
    r = N%16;
    N = N-r;
    __asm__ __volatile__ (						\
      "vmovapd (%[s]), %%ymm0\n\t"					\
      "loop_triad:\n\t"							\
      "vmovupd (%[a]), %%ymm1\n\t"					\
      "vmovupd (%[b]), %%ymm2\n\t"					\
      "vfmadd231pd %%ymm0, %%ymm2, %%ymm1\n\t"                          \
      "vmovupd %%ymm1, (%[c])\n\t"					\
      "vmovupd 32(%[a]), %%ymm3\n\t"					\
      "vmovupd 32(%[b]), %%ymm4\n\t"					\
      "vfmadd231pd %%ymm0, %%ymm4, %%ymm3\n\t"                          \
      "vmovupd %%ymm3, 32(%[c])\n\t"					\
      "vmovupd 64(%[a]), %%ymm5\n\t"					\
      "vmovupd 64(%[b]), %%ymm6\n\t"					\
      "vfmadd231pd %%ymm0, %%ymm6, %%ymm5\n\t"                          \
      "vmovupd %%ymm5, 64(%[c])\n\t"					\
      "vmovupd 96(%[a]), %%ymm7\n\t"					\
      "vmovupd 96(%[b]), %%ymm8\n\t"					\
      "vfmadd231pd %%ymm0, %%ymm8, %%ymm7\n\t"                          \
      "vmovupd %%ymm7, 96(%[c])\n\t"					\
      "add $128, %[a]\n\t"						\
      "add $128, %[b]\n\t"						\
      "add $128, %[c]\n\t"						\
      "sub $16,  %[n]\n\t"						\
      "jnz loop_triad\n\t"						\
      ::[s] "r" (vscale), [a] "r" (A), [b] "r" (B), [c] "r" (C), [n] "r" (N) \
      : "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7", "%xmm8", "memory");
    int i; for(i = N; i<N+r; i++){C[i] = A[i]+scalar*B[i];}
#ifdef _OPENMP
  }
  
#endif
  free(vscale);
}

