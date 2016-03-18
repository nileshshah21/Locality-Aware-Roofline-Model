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

* `PAR=` with values among `{SEQ, OMP}` which compiles whether the sequential (default) version or the parallel version.

* `SIMD=` with values among `{AVX512, AVX, SSE, SSE2, DBL}` which compiles the code with appropriate instructions, for your cpu. DBL option actually compile SSE assembly with the instruction loading the smallest piece of data.

* `DUR=` repeat the benchmark to make it last DUR milliseconds. This is useful in case you want to sample benchmarks performance with external sampling tool.



### Requirements
This soft requires at least hwloc library to be installed.

For multithread result, it is also required to have a recent version of gcc supporting omp reduce pragma.

For now, it only works with intel cpus but one can implement the interface MSC.h with other architectures code.


### Usage
First of all it is required to export the variable BENCHMARK_CPU_FREQ (The frequency of your CPU in Hertz)
```
export BENCHMARK_CPU_FREQ=2100000000
```
Then you need to tell which compiler do you use, in order to compile and run validation benchmarks on the fly:
```
export CC=gcc
```
Here you are ready to play

* Display usage: `./main -h`

* Running benchmark: `./main`

* Running benchmark with validation `./main -v`
Validation consists in writing a list of load/store operation, interleaved with mul/add operation depending on the required operational intensity,
compile and run each benchmark.

* Restrict to a type of operation `./main --load`

* Restrict to a given memory `./main -m L1d:0`

* plot help: `./utils/plot_roofs.bash -h`

* plot output: `./utils/plot_roofs.bash -i input -f gnuplot -t LOAD`


