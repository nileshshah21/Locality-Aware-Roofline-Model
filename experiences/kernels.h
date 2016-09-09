double ddot(const int N,
	    const double *X,
	    const double *Y);

void scale(const int N,
	   const double * X,
	   double * Y,
	   const double scalar);

void triad(const int N,
	   const double * A,
	   const double * B,
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


