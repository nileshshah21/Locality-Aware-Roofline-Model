double ddot(const int N,
	    const double *X,
	    const double *Y);

double blas_ddot(const int N,
		 const double *X,
		 const double *Y);

double avx_ddot(int N,
		double *X,
		double *Y);

void circular_ddot(const int n_iter,
		   size_t buf_size);

void scale(const int N,
	   const double * X,
	   double * Y,
	   const double scalar);

void avx_scale(int N,
	       double *X,
	       double *Y,
	       double scalar);

void triad(const int N,
	   const double * A,
	   const double * B,
	   double * C,
	   const double scalar);

void avx_triad(int N,
	       double * A,
	       double * B,
	       double * C,
	       const double scalar);


void livermore(const int iter,
	       const int n,
	       double * Za,
	       const double * Zb,
	       const double * Zu,
	       const double * Zv,
	       const double * Zr,
	       const double * Zz);


