#ifdef _OPENMP
#include <omp.h>
#endif

double ddot(const int N, const double *X, const double *Y){
  int i;
  double sum = 0;
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for(i=0; i<N; i++){sum += X[i] * Y[i];}
  return sum;
}

void scale(const int N, const double * X, double * Y, const double scalar){
  int i;
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(i=0; i<N; i++) {Y[i] = scalar*X[i];}
}

void triad(const int N, const double * A, const double * B, double * C, const double scalar){
  int i;
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for(i=0; i<N; i++) {C[i] = A[i]+scalar*B[i];}
}

void livermore(const int iter, const int n, double * Za, const double * Zb, const double * Zu, const double * Zv, const double * Zr, const double * Zz){
  int s,j,k;
  double tmp;
  for (s=0; s<iter ; s++) 
  {
#ifdef _OPENMP
#pragma omp parallel for collapse(2)
#endif
    for(j=1;j<n-1;j++)
    {
      for(k=1;k<n-1;k++)
      {
        tmp = Za[((j+1)*n)+k] * Zr[j*n+k] + Za[(j-1)*n+k]*Zb[j*n+k] + Za[j*n+k+1]*Zu[j*n+k] + Za[j*n+k-1] * Zv[j*n+k] + Zz[j*n+k];
	Za[j*n+k] += 0.175 * (tmp - Za[j*n+k]);
      }
    }
  }
}

