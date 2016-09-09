#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sampling.h>
#include "kernels.h"

static int type = TYPE_LOAD;
static char * output = NULL;
static int size = 10000;
static int repeat = 1;

#define do_sample(sample, call, id) do{		\
    roofline_sampling_start(sample);		\
    call;					\
    roofline_sampling_stop(sample);		\
    roofline_sample_print(sample, id);		\
    roofline_sample_clear(sample);		\
  } while(0)

static inline double * new_array(const int N){
  return (double *)malloc(N * sizeof(double));
}

static inline void usage(const char * argv0){
  fprintf(stderr, "%s (-s <size>) (-r <repeat>) (-o <output>) -t (<type={load, store}>) (-h)\n", argv0);
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
    } else if(!strcmp(argv[i], "-t")){
      if(i+1==argc){usage(argv0);}
      type = !strcmp(argv[i+1], "store") ? TYPE_STORE : TYPE_LOAD; i++;
    } else if(!strcmp(argv[i], "-h")){
      usage(argv0);
    }
  }
}

int main(int argc, char ** argv){
  parse_args(argc, argv);
  
  double *Za = new_array(size*size);
  double *Zb = new_array(size*size);
  double *Zu = new_array(size*size);
  double *Zv = new_array(size*size);
  double *Zr = new_array(size*size);
  double *Zz = new_array(size*size);

  roofline_sampling_init(output);
  struct roofline_sample * s = new_roofline_sample(type);

  int i;

  for(i=0;i<repeat;i++)
    do_sample(s, ddot(size*size, Za, Zb), "ddot");
  printf("ddot done.\n");

  for(i=0;i<repeat;i++)
    do_sample(s, scale(size*size, Za, Zb, 2.34), "scale");
  printf("scale done.\n");

  for(i=0;i<repeat;i++)
    do_sample(s, triad(size*size, Za, Zb, Zu, 2.34), "triad");
  printf("triad done.\n");

  for(i=0;i<repeat;i++)
    do_sample(s, livermore(100, size, Za, Zb, Zu, Zv, Zr, Zz), "livermore");
  printf("livermore done.\n");

  
  free(Za);
  free(Zb);
  free(Zu);
  free(Zv);
  free(Zr);
  free(Zz);
  delete_roofline_sample(s);
  roofline_sampling_fini();
  return EXIT_SUCCESS;
}


