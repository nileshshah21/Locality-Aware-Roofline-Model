#include <math.h>
#include "roofline.h"

/* options */
static char * output = NULL;
static int validate = 0;
static char * mem_str = NULL;
static hwloc_obj_t mem = NULL;
static int load = 0, store = 0, copy = 0;
static int hyperthreading = 0;
int per_thread = 0;

static void usage(char * argv0){
    printf("%s -h  -v -l -s -c -m <hwloc_ibj:idx> -o <output> -ht\n", argv0);
    printf("%s --help --validate --load --store --copy --memory <hwloc_obj:idx> --output <output> --with-hyperthreading\n", argv0);
    
    exit(EXIT_SUCCESS);
}

static void parse_args(int argc, char ** argv){
    int i;

    i= 0;
    while(++i < argc){
	if(!strcmp(argv[i],"--help") || !strcmp(argv[i],"-h"))
	    usage(argv[0]);
	else if(!strcmp(argv[i],"--validate") || !strcmp(argv[i],"-v"))
	    validate = 1;
	else if(!strcmp(argv[i],"--load") || !strcmp(argv[i],"-l"))
	    load = 1;
	else if(!strcmp(argv[i],"--store") || !strcmp(argv[i],"-s"))
	    store = 1;
	else if(!strcmp(argv[i],"--copy") || !strcmp(argv[i],"-c"))
	    copy = 1;
	else if(!strcmp(argv[i],"--with-hyperthreading") || !strcmp(argv[i],"-ht"))
	    hyperthreading = 1;
	else if(!strcmp(argv[i],"--per-thread") || !strcmp(argv[i],"-pt"))
	    per_thread = 1;
	else if(!strcmp(argv[i],"--memory") || !strcmp(argv[i],"-m")){
	    mem_str = argv[++i];
	}
	else if(!strcmp(argv[i],"--output") || !strcmp(argv[i],"-o")){
	    output = argv[++i];
	}
    }
    if(!load && !store && !copy){
	load =1; store=1;
    }
}

static FILE * open_output(char * out){
    FILE * fout;

    if(out == NULL)
	return NULL;
    fout = fopen(out,"w");
    if(fout == NULL) perrEXIT("fopen");
    return fout;
}

static void roofline_mem_bench(FILE * out, hwloc_obj_t memory){
    double oi;

    oi = 0;
    /* Benchmark load */
    if(load){
	roofline_bandwidth(out, memory, ROOFLINE_LOAD);
	if(validate)
	    for(oi = pow(2,-12); oi < pow(2,6); oi*=2){
		roofline_oi(out, memory, ROOFLINE_LOAD, oi);
	    }
    }
    /* Benchmark store */
    if(store){
	roofline_bandwidth(out, memory, ROOFLINE_STORE);
	if(validate)
	    for(oi = pow(2,-12); oi < pow(2,6); oi*=2){
		roofline_oi(out, memory, ROOFLINE_STORE, oi);
	    }
    }
    if(copy){
	roofline_bandwidth(out, memory, ROOFLINE_COPY);
	if(validate)
	    for(oi = pow(2,-12); oi < pow(2,6); oi*=2){
		roofline_oi(out, memory, ROOFLINE_COPY, oi);
	    }
    }
}

int main(int argc, char * argv[]){
    FILE * out;
    char info[128];

    memset(info,0,sizeof(info));
    parse_args(argc,argv);

    if(roofline_lib_init(hyperthreading)==-1)
	errEXIT("roofline library init failure");

    if(mem_str != NULL){
	mem = roofline_hwloc_parse_obj(mem_str);
	if(mem == NULL)
	errEXIT("Unrecognized object");
	if(!roofline_hwloc_obj_is_memory(mem))
	mem = roofline_hwloc_get_previous_memory(mem);
    }

    out = open_output(output);
    if(out==NULL) out = stdout;

    /* print header */
    snprintf(info,sizeof(info), "%5s", "info");
    roofline_print_header(out, info);

    /* roofline for flops */
    roofline_fpeak(out);
    
    /* roofline every memory obj */
    if(mem == NULL){
	while((mem = roofline_hwloc_get_next_memory(mem)) != NULL){
	    roofline_mem_bench(out, mem);
	}
    }
    else
	roofline_mem_bench(out, mem);
    
    fclose(out);
    roofline_lib_finalize();
    return EXIT_SUCCESS;
}


