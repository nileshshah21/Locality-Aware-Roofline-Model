#include <math.h>
#include "roofline.h"

/* options */
static char * output = NULL;            /* Where to print output */
static char * mem_str = NULL;           /* Do we restrict memory to one memory */
static hwloc_obj_t mem = NULL;          /* Do we restrict memory to one memory */
static int hyperthreading = 0;          /* If compiled with openmp do we use hyperthreading */
static double oi = 0;                   /* -1 => perform validation benchmarks, >0 => perform validation benchmarks on value */

static void usage(char * argv0){
    printf("%s <options...>\n\n", argv0);
    printf("OPTIONS:\n");
    printf("\t-h, --help: print this help message\n");
    printf("\t-v, --validate: perform roofline validation checking. This will run benchmark for several operational intensity trying to hit the roofs.\n");
    printf("\t-oi, --operational-intensity: perform roofline validation checking on current operational intensity.\n");
    printf("\t-t, --type  <\"LOAD|LOAD_NT|STORE|STORE_NT|2LD1ST|MUL|ADD|MAD\">: choose the roofline types among load, load_nt, store, store_nt, or 2loads/1store for memory, add, mul, and fma for fpeak.\n");
    printf("\t-m, --memory <hwloc_ibj:idx>: restrict memory roofline to a single memory in the hierarchiy.\n");
    printf("\t-o, --output <output>: Set output file to write restults.\n");
#if defined(_OPENMP)
    printf("\t-ht, --with-hyperthreading: use hyperthreading for benchmarks\n");
#endif    
    exit(EXIT_SUCCESS);
}

static void parse_type(char * type){
    char * t = strtok(type,"|");
    while(t!=NULL){
	roofline_types = (roofline_types | roofline_type_from_str(t));
	t = strtok(NULL,"|");
    }
}

static void parse_args(int argc, char ** argv){
    int i;

    i= 0;
    while(++i < argc){
	if(!strcmp(argv[i],"--help") || !strcmp(argv[i],"-h"))
	    usage(argv[0]);
	else if(!strcmp(argv[i],"--validate") || !strcmp(argv[i],"-v"))
	    oi = -1;
	else if(!strcmp(argv[i],"--operational-intensity") || !strcmp(argv[i],"-oi"))
	    oi = atof(argv[++i]);
	else if(!strcmp(argv[i],"--type") || !strcmp(argv[i],"-t")){
	    parse_type(argv[++i]);
	}
#if defined(_OPENMP)	
	else if(!strcmp(argv[i],"--with-hyperthreading") || !strcmp(argv[i],"-ht"))
	    hyperthreading = 1;
#endif
	else if(!strcmp(argv[i],"--memory") || !strcmp(argv[i],"-m")){
	    mem_str = argv[++i];
	}
	else if(!strcmp(argv[i],"--output") || !strcmp(argv[i],"-o")){
	    output = argv[++i];
	}
    }
    if(roofline_types == 0){
	roofline_types = ROOFLINE_2LD1ST|ROOFLINE_LOAD|ROOFLINE_STORE_NT|ROOFLINE_MAD;
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

int main(int argc, char * argv[]){
    FILE * out;
    parse_args(argc,argv);

    if(roofline_lib_init(hyperthreading)==-1)
	errEXIT("roofline library init failure");

    if(mem_str != NULL){
	mem = roofline_hwloc_parse_obj(mem_str);
	if(mem == NULL)
	errEXIT("Unrecognized object");
	if(!roofline_hwloc_obj_is_memory(mem))
	mem = roofline_hwloc_get_under_memory(mem);
    }

    out = open_output(output);
    if(out==NULL) out = stdout;

    /* print header */
    roofline_print_header(out);

    /* roofline for flops */
    roofline_flops(out, roofline_types);
    
    /* roofline every memory obj */
    if(mem == NULL){
	while((mem = roofline_hwloc_get_next_memory(mem)) != NULL){
	    if(oi>0){
		roofline_oi(out, mem, roofline_types, oi);
	    }
	    else{
		roofline_bandwidth(out, mem, roofline_types);
	    }
	}
	while((mem = roofline_hwloc_get_next_memory(mem)) != NULL){
	    if(oi<0){
		double op_int = oi;
		for(op_int = pow(2,-10); op_int < pow(2,6); op_int*=2){
		    roofline_oi(out, mem, roofline_types, op_int);
		}
	    }
	}
    }
    else{
	if(oi>0){
	    roofline_oi(out, mem, roofline_types, oi);
	}
	else{
	    roofline_bandwidth(out, mem, roofline_types);
	    if(oi<0){
		double op_int = oi;
		for(op_int = pow(2,-10); op_int < pow(2,6); op_int*=2){
		    roofline_oi(out, mem, roofline_types, op_int);
		}
	    }
	}
    }
    
    fclose(out);
    roofline_lib_finalize();
    return EXIT_SUCCESS;
}


