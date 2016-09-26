#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sampling.h>
#include "kernels.h"

static char * output = NULL;
static size_t size = 10000;
static long repeat = 1;

#define do_sample(sample, call, id) do{		\
    int i;					\
    roofline_sampling_start(sample);		\
    call;					\
    roofline_sampling_stop(sample);		\
    for(i=0;i<repeat;i++){			\
      roofline_sample_clear(sample);		\
      roofline_sampling_start(sample);		\
      call;					\
      roofline_sampling_stop(sample);		\
      roofline_sample_print(sample , id);	\
    }						\
    printf("%s done.\n", id);			\
  } while(0)

static inline double * new_array(const int N){
  int i;
  double * ret = malloc(N * sizeof(double));
  /* memset(ret, 0, N * sizeof(double)); */
  for(i=0;i<N;i++) ret[i] = rand();
  return ret;
}

static inline void usage(const char * argv0){
  fprintf(stderr, "%s (-s <size (square)>) (-r <repeat>) (-o <output>) (-h)\n", argv0);
  exit(EXIT_SUCCESS);
}

static void parse_args(int argc, char ** argv){
  int i;
  const char * argv0 = argv[0];
  argc--; argv++;
  for(i=0; i<argc; i++){
    if(!strcmp(argv[i], "-s")){
      if(i+1==argc){usage(argv0);}
      size = strtoul(argv[i+1], NULL, 10); i++;
    } else if(!strcmp(argv[i], "-o")){
      if(i+1==argc){usage(argv0);}
      output = argv[i+1]; i++;
    } else if(!strcmp(argv[i], "-r")){
      if(i+1==argc){usage(argv0);}
      repeat = atoi(argv[i+1]); i++;
    } else if(!strcmp(argv[i], "-h")){
      usage(argv0);
    }
  }
}

int main(int argc, char ** argv){
  parse_args(argc, argv);

  /* unsigned n = size/sizeof(double); */
  /* unsigned sq = sqrt(n); */
  /* sq = sq - sq%sizeof(double); */
  
  /* double *Za = new_array(n); */
  /* double *Zb = new_array(n); */
  /* double *Zu = new_array(n); */
  /* double *Zv = new_array(n); */
  /* double *Zr = new_array(n); */
  /* double *Zz = new_array(n); */
  
  /* roofline_sampling_init(output); */
  /* struct roofline_sample * ld = new_roofline_sample(TYPE_LOAD); */
  /* struct roofline_sample * st = new_roofline_sample(TYPE_STORE); */

  /* do_sample(ld, printf("ddot: %lf\n", ddot(n, Za, Zb)), "ddot"); */
  /* do_sample(st, printf("ddot: %lf\n", ddot(n, Za, Zb)), "ddot"); */
  
  /* do_sample(ld, printf("ddot: %lf\n", blas_ddot(n, Za, Zb)), "blas_ddot"); */
  /* do_sample(st, printf("ddot: %lf\n", blas_ddot(n, Za, Zb)), "blas_ddot"); */

  /* do_sample(ld, printf("ddot: %lf\n", avx_ddot(n, Za, Zb)), "avx_ddot"); */
  /* do_sample(st, printf("ddot: %lf\n", avx_ddot(n, Za, Zb)), "avx_ddot"); */

  /* do_sample(ld, scale(n, Za, Zb, 2.34), "scale"); */
  /* do_sample(st, scale(n, Za, Zb, 2.34), "scale"); */

  /* do_sample(ld, avx_scale(n, Za, Zb, 2.34), "avx_scale"); */
  /* do_sample(st, avx_scale(n, Za, Zb, 2.34), "avx_scale"); */
  
  /* do_sample(ld, triad(n, Za, Zb, Zu, 2.34), "triad"); */
  /* do_sample(st, triad(n, Za, Zb, Zu, 2.34), "triad"); */

  /* do_sample(ld, avx_triad(n, Za, Zb, Zu, 2.34), "avx_triad"); */
  /* do_sample(st, avx_triad(n, Za, Zb, Zu, 2.34), "avx_triad"); */

  circular_ddot(repeat, size);
  
  /* free(Za); */
  /* free(Zb); */
  /* free(Zu); */
  /* free(Zv); */
  /* free(Zr); */
  /* free(Zz); */
  /* delete_roofline_sample(ld); */
  /* delete_roofline_sample(st); */
  /* roofline_sampling_fini(); */
  return EXIT_SUCCESS;
}


