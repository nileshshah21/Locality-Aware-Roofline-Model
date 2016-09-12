# LARM (Locality Aware Roofline Model)
### Benchmark and plot your system bounds on the roofline model.
  The ever growing complexity of high performance computing systems imposes significant challenges to exploit as much as
  possible their computational and communication resources.
  Recently, the Cache-aware Roofline Model has has gained popularity due to its simplicity modeling multi-cores with complex memory
  hierarchy, characterizing applications' bottlenecks, and quantifying achieved or remaining improvements.
  With this project, we push this model a step further to model NUMA and heterogeneous memories with a handy tool.

  The repository contain the material to build the tool to benchmark your platform, to build a library to collect roofline metrics of your application, and finally a script to present the results.
  
![](roofline_chart.png?raw=true)

This plot shows load instructions' rooflines (lines) and validation kernels (points) hitting the measured bandwidth.

### Installation
```
git clone https://github.com/NicolasDenoyelle/LARM-Locality-Aware-Roofline-Model-.git
cd LARM-Locality-Aware-Roofline-Model-/src
make
```

Several options can be set in [Makefile](./src/Makefile)
* `LAST=` repeat the benchmark to make it last LAST milliseconds. This is useful in case you want to sample benchmarks performance with external sampling tool. Making longer benchmarks can give more stable results.

* `N_SAMPLES=`Number of sample to take for each memory benchmark. The samples scale is exponential, and bounds are automatically found according to memory sizes.

* `PAPI=no` Change to yes to compile librfsampling. This library allows to use functions defined in [sampling.h](./src/sampling.h),
in order to output performance results from an application code, drawable on the roofline chart.

* `OMP_FLAG=` Change to your compiler openmp flag to enable parallel benchmark.


### Requirements

* This soft requires at least a recent enough hwloc library to be installed.
* The library using hardware counters uses papi, but it is not compiled by default.
* For now, it only works with intel processors but one can implement the interface MSC.h with other architectures code.
Be aware that though an optional [generic implementation](./src/MSC/generic.c) (not used by default) is present, peak floating point benchmarks are not reliable. Achieving peak
performance with compiled code is a very hard task performed by very few finely tuned codes such as cblas_dgemm and can hardly be
achieved in a generic way.
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


