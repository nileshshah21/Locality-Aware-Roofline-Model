#Openmp Bind to Cores
export OMP_NUM_THREADS=$(lstopo --only Core --restrict `hwloc-calc Node:0`| wc -l)
export GOMP_CPU_AFFINITY=""
for i in $(seq 0 1 $(($OMP_NUM_THREADS-1))); do
    GOMP_CPU_AFFINITY="$GOMP_CPU_AFFINITY $(lstopo --physical --restrict `hwloc-calc Core:$i` --only PU | awk -F'#' '{print $2; exit}')"
done

