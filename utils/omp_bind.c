#include <hwloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define GATHER 0
#define SCATTER 1

static hwloc_topology_t topology = NULL;
static hwloc_obj_t root = NULL;
static hwloc_obj_type_t leaf_type = HWLOC_OBJ_CORE;
static int bind = 0;

static void usage(char * argv0){
    fprintf(stderr, "%s --bind <scatter|gather> --restrict <restrict obj> --leaf <PU|Core>\n", argv0);
    fprintf(stderr, "print GOMP_CPU_AFFINITY to bind one thread per processing unit.\n");
    fprintf(stderr, "Gather binds each thread to the next logical PU/Core (depending on leaf option).\n");
    fprintf(stderr, "\tIf leaf=Core, then bind first PU of each core then second PU of each core...\n");
    fprintf(stderr, "Scatter binds each thread to a PU such they are granted a maximum amount of exclusive nodes in the topology.\n");
    fprintf(stderr, "\tOf course with full machine subscribing the amount of node per thread is minimum.\n");
    fprintf(stderr, "\tbut the numbering allows to increase exclusive resources and decrease locality when the thread count decreases.\n");
    exit(EXIT_SUCCESS);
}

static hwloc_obj_t parse_obj(char* arg){
    hwloc_obj_type_t type; 
    char * name;
    int err, depth; 
    char * idx;
    int logical_index;
    name = strtok(arg,":");
    if(name==NULL)
	return NULL;
    err = hwloc_type_sscanf_as_depth(name, &type, topology, &depth);
    if(err == HWLOC_TYPE_DEPTH_UNKNOWN){
	fprintf(stderr,"type %s cannot be found, level=%d\n",name,depth);
	return NULL;
    }
    if(depth == HWLOC_TYPE_DEPTH_MULTIPLE){
	    fprintf(stderr,"type %s multiple caches match for\n",name);
	    return NULL;
    }
    idx = strtok(NULL,":");
    logical_index = 0;
    if(idx!=NULL)
	logical_index = atoi(idx);
    return hwloc_get_obj_by_depth(topology,depth,logical_index);
}

static void parse_args(int argc, char ** argv){
    int i;
    for(i=1;i<argc;i++){
	if(!strcmp(argv[i], "--bind")){
	    i++;
	    if(!strcmp(argv[i], "scatter"))
		bind = SCATTER;
	    else if(!strcmp(argv[i], "gather"))
		bind = GATHER;
	    else 
		usage(argv[0]);
	}
	else if(!strcmp(argv[i], "--leaf")){
	    i++;
	    if(!strcmp(argv[i], "PU"))
		leaf_type = HWLOC_OBJ_PU;
	    else if(!strcmp(argv[i], "Core"))
		leaf_type = HWLOC_OBJ_CORE;
	    else 
		usage(argv[0]);
	}
	else if(!strcmp(argv[i], "--restrict")){
	    root = parse_obj(argv[++i]);
	    if(root == NULL)
		exit(EXIT_SUCCESS);
	}
	else
	    usage(argv[0]);
    }
}

static void output_gather(){
    int i, n_leaves, n_PU;
    hwloc_obj_t leaf = NULL;

    if(leaf_type == HWLOC_OBJ_PU){
	while(leaf = hwloc_get_next_obj_inside_cpuset_by_type(topology, root->cpuset, leaf_type, leaf))
	    printf("%d ", leaf->os_index);
    }
    else if(leaf_type == HWLOC_OBJ_CORE){
	n_PU     = hwloc_get_nbobjs_inside_cpuset_by_type(topology,root->cpuset, HWLOC_OBJ_PU);
	n_leaves = hwloc_get_nbobjs_inside_cpuset_by_type(topology,root->cpuset, HWLOC_OBJ_CORE);
	for(i=0;i<n_PU;i++){
	    leaf = hwloc_get_obj_inside_cpuset_by_type(topology, root->cpuset, leaf_type, i%n_leaves);
	    printf("%d ", leaf->children[(i/n_leaves)%leaf->arity]->os_index);
	}	    
    }
    printf("\n");
}

static void output_scatter(){
    int i, logical, depth, n_leaves, leaf_depth;
    hwloc_obj_t leaf;

    n_leaves = hwloc_get_nbobjs_inside_cpuset_by_type(topology,root->cpuset, HWLOC_OBJ_PU);
    leaf_depth = hwloc_get_type_depth(topology,HWLOC_OBJ_PU);

    for(i=0;i<n_leaves;i++){
	leaf = root;
	logical = i;
	for(depth=root->depth;depth<leaf_depth;depth++){
	    leaf = leaf->children[logical%leaf->arity];
	    logical /= leaf->parent->arity;
	}
	printf("%d ", leaf->os_index);
    }

    printf("\n");
}

int main(int argc, char ** argv){
    /* Initialize topology */
    if(hwloc_topology_init(&topology) ==-1){
	fprintf(stderr, "hwloc_topology_init failed.\n");
	return -1;
    }
    hwloc_topology_set_icache_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_STRUCTURE);
    if(hwloc_topology_load(topology) ==-1){
	fprintf(stderr, "hwloc_topology_load failed.\n");
	return -1;
    }
    root = hwloc_get_root_obj(topology);
    parse_args(argc,argv);
    
    if(bind==GATHER)
	output_gather(); 
    else if(bind==SCATTER)
	output_scatter(); 
	
    hwloc_topology_destroy(topology);
    return EXIT_SUCCESS;
}
