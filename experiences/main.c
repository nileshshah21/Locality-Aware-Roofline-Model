#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sampling.h>
#include "kernels.h"

static int eventset;
static struct roofline_sample s;
static char * output = NULL;
static int size = 10000;
static int repeat = 1;

#define do_sample(call, id) do{			\
    roofline_sampling_start(eventset, &s);	\
    call;					\
    roofline_sampling_stop(eventset, &s);	\
    roofline_sample_print(&s , id);		\
  } while(0)

static inline double * new_array(const int N){
  return (double *)malloc(N * sizeof(double));
}

static inline void usage(const char * argv0){
  fprintf(stderr, "%s (-s <size>) (-r <repeat>) (-o <output>) (-h)\n", argv0);
  exit(EXIT_SUCCESS);
}

static void parse_args(int argc, char ** argv){
  int i;
  const char * argv0 = argv[0];
  argc--; argv++;
  for(i=0; i<argc; i++){
    if(!strcmp(argv[i], "-s")){
      if(i+1==argc){usage(argv0);}
      size = atoi(argv[i+1]); i++;
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

  roofline_sampling_init(NULL);
  roofline_eventset_init(&eventset);

  double *Za = new_array(size*size);
  double *Zb = new_array(size*size);
  double *Zu = new_array(size*size);
  double *Zv = new_array(size*size);
  double *Zr = new_array(size*size);
  double *Zz = new_array(size*size);
  printf("init done.\n");

  int i;

  for(i=0;i<repeat;i++)
    do_sample(ddot(size, Za, Zb), "ddot");
  printf("ddot done.\n");

  for(i=0;i<repeat;i++)
    do_sample(scale(size, Za, Zb, 2.34), "scale");
  printf("scale done.\n");

  for(i=0;i<repeat;i++)
    do_sample(triad(size, Za, Zb, Zu, 2.34), "scale");
  printf("scale done.\n");

  for(i=0;i<repeat;i++)
    do_sample(livermore(100, size, Za, Zb, Zu, Zv, Zr, Zz), "scale");
  printf("livermore done.\n");

  
  free(Za);
  free(Zb);
  free(Zu);
  free(Zv);
  free(Zr);
  free(Zz);
  roofline_sampling_fini();
  roofline_eventset_destroy(&eventset);
  return EXIT_SUCCESS;
}


