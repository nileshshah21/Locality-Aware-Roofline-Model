#include <sys/time.h>
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

