# LARM (Locality Aware Roofline Model)
### Brief
This repository holds fast tools to measure, validate and plot the different bandwidth of a CPU system.
Benchmarks are hand-written assembly loops with known exact number of load,store,mul,add instructions.
The only measured things is the number of cycles elapsed. 
For each memory(including cache) in the memory hierarchy a buffer is loaded a fixed number of time, then the median value is saved.
For each memory, several size fitting the memory space are loaded, and again, the median value is saved as well as the standard deviation.

![](roofline_chart.png?raw=true)

This plot output shows load instructions' rooflines (lines) and validation kernels (points) hitting the measured bandwidth.

### Installation
```
git clone https://github.com/NicolasDenoyelle/LARM-Locality-Aware-Roofline-Model-.git
cd LARM-Locality-Aware-Roofline-Model-/src
make
```

Several options can be set in [Makefile](./src/Makefile)
* `LAST=` repeat the benchmark to make it last DUR milliseconds. This is useful in case you want to sample benchmarks performance with external sampling tool. Making longer benchmarks can give more stable results.

* `PAPI=no` Change to yes to compile librfsampling. This library allows to use functino defined in [sampling.h](./src/sampling.h),
in order to output performance results from an application code, drawable on the roofline chart.

* `OMP_FLAG=` Change to your compiler openmp flag to enable parallel benchmark.


### Requirements

* This soft requires at least a recent enough hwloc library to be installed.
* The library using hardware counters to export results also uses papi, but it is not compiled by default.
* For now, it only works with intel processors but one can implement the interface MSC.h with other architectures code.
* R to use the [plot script](./utils/plot_roofs.bash).

#### /!\ Important: 
Generally speaking, if you want to get relevant results on such benchmarks, you have to assert that options like turbo-boost are disabled and
the cpu frequency is set.
Therefore, it is required to export the variable BENCHMARK_CPU_FREQ (The frequency of your CPU in Hertz)
```
export CPU_FREQ=2100000000
```
Here you are ready to play

* Display usage: `./roofline -h`

* Running benchmark: `./roofline`

* Running benchmark with validation `./roofline -v`

Validation consists in writing a list of load/store operations, interleaved with mul/add operations depending on the required operational intensity,
compile and run each benchmark.

* plot help: `./utils/plot_roofs.bash -h`

* plot output: `./utils/plot_roofs.bash -i input -t LOAD`


