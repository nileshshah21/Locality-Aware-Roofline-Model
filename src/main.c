#include <math.h>
#include "roofline.h"

/* options */
static char * output = NULL;            /* Where to print output */
static int validate = 0;                /* Do we perform validation benchmarks */
static char * mem_str = NULL;           /* Do we restrict memory to one memory */
static hwloc_obj_t mem = NULL;          /* Do we restrict memory to one memory */
static int hyperthreading = 0;          /* If compiled with openmp do we use hyperthreading */

static void usage(char * argv0){
#if defined(_OPENMP)
    printf("%s -h     -v         -t     <\"LOAD|LOAD_NT|STORE|STORE_NT|MUL|ADD|MAD\"> -m       <hwloc_ibj:idx> -o       <output> -ht                   -pt\n", argv0);
    printf("%s --help --validate --type <\"LOAD|LOAD_NT|STORE|STORE_NT|MUL|ADD|MAD\"> --memory <hwloc_obj:idx> --output <output> --with-hyperthreading --per-thread\n", argv0);
#else
    printf("%s -h     -v         -t     <\"LOAD|LOAD_NT|STORE|STORE_NT|MUL|ADD|MAD\"> -m       <hwloc_ibj:idx> -o       <output>\n", argv0);
    printf("%s --help --validate --type <\"LOAD|LOAD_NT|STORE|STORE_NT|MUL|ADD|MAD\"> --memory <hwloc_obj:idx> --output <output>\n", argv0);

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
	    validate = 1;
	else if(!strcmp(argv[i],"--type") || !strcmp(argv[i],"-t")){
	    parse_type(argv[++i]);
	}
#if defined(_OPENMP)	
	else if(!strcmp(argv[i],"--with-hyperthreading") || !strcmp(argv[i],"-ht"))
	    hyperthreading = 1;
	else if(!strcmp(argv[i],"--per-thread") || !strcmp(argv[i],"-pt"))
	    per_thread = 1;
#endif
	else if(!strcmp(argv[i],"--memory") || !strcmp(argv[i],"-m")){
	    mem_str = argv[++i];
	}
	else if(!strcmp(argv[i],"--output") || !strcmp(argv[i],"-o")){
	    output = argv[++i];
	}
    }
    if(roofline_types == 0){
	roofline_types = ROOFLINE_LOAD|ROOFLINE_STORE|ROOFLINE_MAD;
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
	mem = roofline_hwloc_get_under_memory(mem);
    }

    out = open_output(output);
    if(out==NULL) out = stdout;

    /* print header */
    snprintf(info,sizeof(info), "%5s", "info");
    roofline_print_header(out, info);

    /* roofline for flops */
    roofline_flops(out, roofline_types);
    
    /* roofline every memory obj */
    if(mem == NULL){
	while((mem = roofline_hwloc_get_next_memory(mem)) != NULL){
	    roofline_bandwidth(out, mem, roofline_types);
	    if(validate){
		double oi;
		for(oi = pow(2,-10); oi < pow(2,6); oi*=2){
		    roofline_oi(out, mem, roofline_types, oi);
		}
	    }
	}
    }
    else{
	roofline_bandwidth(out, mem, roofline_types);
	if(validate){
	    double oi;
	    for(oi = pow(2,-10); oi < pow(2,6); oi*=2){
		roofline_oi(out, mem, roofline_types, oi);
	    }
	}
    }
    
    fclose(out);
    roofline_lib_finalize();
    return EXIT_SUCCESS;
}


