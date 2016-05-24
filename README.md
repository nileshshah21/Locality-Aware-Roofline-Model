# LARM (Locality Aware Roofline Model)
### Brief
This repository holds fast tools to measure, validate and plot the different bandwidth of a CPU system.
Benchmarks are hand-written assembly loops with known exact number of load,store,mul,add instructions.
The only measured things is the number of cycles elapsed. 
For each memory(including cache) in the memory hierarchy a buffer is loaded a fixed number of time, then the median value is saved.
For each memory, several size fitting the memory space are loaded, and again, the median value is saved as well as the standard deviation.

![](roofline_chart.png?raw=true)

This plot output shows store rooflines (lines) and validation kernels (points) hitting the measured bandwidth.

### Installation
```
git clone https://github.com/NicolasDenoyelle/LARM-Locality-Aware-Roofline-Model-.git
cd LARM-Locality-Aware-Roofline-Model-/src
make
```
Their are 3 options which can be appended to `make`:

* `PAR=` with values among `{SEQ, OMP}` which compiles whether the sequential version or the parallel (default) version.

* `DUR=` repeat the benchmark to make it last DUR milliseconds. This is useful in case you want to sample benchmarks performance with external sampling tool.



### Requirements
This soft requires at least hwloc library to be installed.

For multithread result, it is also required to have a compiler with openmp support.
It is recomended that you use the script `utils/omp_set_affinity.sh` to properly set GOMP openmp threads binding.
```
source utils/omp_set_affinity.sh	
```
The benchmark will set the number of threads depending on the memory level to test afterward.

For now, it only works with intel cpus but one can implement the interface MSC.h with other architectures code.

#### /!\ Important: 
Generally speaking, if you want to get relevant results on such benchmarks, you have to assert that options like turbo-boost are disabled and
the cpu frequency is set.
Therefore, it is required to export the variable BENCHMARK_CPU_FREQ (The frequency of your CPU in Hertz)
```
export BENCHMARK_CPU_FREQ=2100000000
```
Here you are ready to play

* Display usage: `./main -h`

* Running benchmark: `./main`

* Running benchmark using hyperthreading: `./main -ht`

* Running benchmark with validation `./main -v`
Validation consists in writing a list of load/store operations, interleaved with mul/add operations depending on the required operational intensity,
compile and run each benchmark.

* Restrict to a type of operation `./main --load`

* Restrict to a given memory `./main -m L1d:0`

* plot help: `./utils/plot_roofs.bash -h`

* plot output: `./utils/plot_roofs.bash -i input -f gnuplot -t LOAD`


