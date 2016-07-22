#include <sys/time.h>
#include <math.h>
#include "roofline.h"

unsigned roofline_PGCD(unsigned a, unsigned b){
    unsigned max,min, r;
    max = roofline_MAX(a,b);
    min = roofline_MIN(a,b);
    while((r=max%min)!=0){max = min; min = r;}
    return min;
}

unsigned roofline_PPCM(unsigned a, unsigned b){
    return a*b/roofline_PGCD(a,b);
}

double roofline_output_sd(struct roofline_sample_out * out, unsigned n){
    unsigned i;
    double median = roofline_output_median(out, n);
    double sd = 0, throughput;
    if(n < 2)
        return 0;

    for(i=0;i<n;i++){
	throughput = (double)(out[i].instructions) / (double)(out[i].ts_end - out[i].ts_start);
	throughput = throughput - median;
	throughput *= throughput;
	sd += throughput;
    }
    return sqrt(sd)/n;
}

int comp_roofline_throughput(void * a, void * b){
    struct roofline_sample_out a_out, b_out;
    float val_a, val_b;
    a_out = (*(struct roofline_sample_out *)a);
    b_out = (*(struct roofline_sample_out *)b);
    if(a_out.ts_end == a_out .ts_start)
	val_a = 0;
    else
	val_a = a_out.instructions / (float)(a_out.ts_end - a_out .ts_start);
    if(b_out.ts_end == b_out .ts_start)
	val_b = 0;
    else
	val_b = b_out.instructions / (float)(b_out.ts_end - b_out .ts_start);
    if( val_a < val_b )
	return -1;
    else if(val_a > val_b)
	return 1;
    else
	return 0;
}

int roofline_output_median(struct roofline_sample_out * samples, size_t n){
    qsort(samples, n, sizeof(*samples), (__compar_fn_t)comp_roofline_throughput);
    return (int)(n/2);
}

float roofline_output_mean(struct roofline_sample_out * samples, size_t n){
    float sum = 0;
    size_t i = n;
    while(i--)
	sum+=( (float)(samples[i].instructions) / (float)(samples[i].ts_end-samples[i].ts_start) );
    return sum/n;
}

int roofline_output_max(struct roofline_sample_out * samples, size_t n){
    int ret;
    struct roofline_sample_out max;

    ret = 0;
    max = samples[--n];

    while(n--){
	if(comp_roofline_throughput(&max,&samples[n]) == -1){ 
	    max = samples[n];
	    ret = n;
	}
    }
    return ret;
}

int roofline_output_min(struct roofline_sample_out * samples, size_t n){
    int ret;
    struct roofline_sample_out min;

    ret = 0;
    min = samples[--n];
    while(n--){
	if(comp_roofline_throughput(&min,&samples[n]) == 1){ 
	    min = samples[n];
	    ret = n;
	}
    }
    return ret;
}

long roofline_autoset_loop_repeat(void (* bench_fun)(const struct roofline_sample_in *, struct roofline_sample_out *, int), struct roofline_sample_in * in, int type, long ms_dur, unsigned long min_rep){
    float mul, tv_ms;
    struct timespec ts, te;
    struct roofline_sample_out out;

    in->loop_repeat = 1; 
    mul = 0;
    tv_ms = 0;
    while(tv_ms < ms_dur){
	clock_gettime(CLOCK_MONOTONIC, &ts);
	bench_fun(in,&out, type);
	clock_gettime(CLOCK_MONOTONIC, &te);
	tv_ms = (te.tv_sec-ts.tv_sec)*1e3 + (float)(te.tv_nsec-ts.tv_nsec)/1e6;
	/* printf("ms_dur = %ld, tv ms = %f\n", ms_dur, tv_ms); */
	if( tv_ms < ms_dur ){
	    mul = (float)ms_dur/tv_ms;
	    if((long)mul <= 1)
		in->loop_repeat *= 2;
	    else
		in->loop_repeat *= mul;
	}
    }

    in->loop_repeat = roofline_MAX(min_rep, in->loop_repeat);
    return tv_ms;
}


double
roofline_repeat_bench(void (* bench)(const struct roofline_sample_in *, struct roofline_sample_out *, int), 
		      struct roofline_sample_in * in, struct roofline_sample_out * out, int type,
		      int (* bench_stat)(struct roofline_sample_out * , size_t))
{
    unsigned i;
    struct roofline_sample_out * samples; 
    double sd; 

    roofline_alloc(samples,sizeof(*samples)*BENCHMARK_REPEAT);
    for(i=0;i<BENCHMARK_REPEAT;i++){
	roofline_output_clear(&(samples[i]));
	bench(in,&(samples[i]), type);
    }    
    /* sort results */
    *out = samples[bench_stat(samples, i)];
    sd = roofline_output_sd(samples,i);
    free(samples);
    return sd;
}

