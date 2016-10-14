#LARM (Locality Aware Roofline Model)
### Benchmark and plot your system bounds on the roofline model.
  The ever growing complexity of high performance computing systems imposes significant challenges to exploit as much as
  possible their computational and communication resources.
  Recently, the Cache-aware Roofline Model has has gained popularity due to its simplicity modeling multi-cores with complex memory
  hierarchy, characterizing applications' bottlenecks, and quantifying achieved or remaining improvements.
  With this project, we push this model a step further to model NUMA and heterogeneous memories with a handy tool.

  The repository contain the material to build the tool to benchmark your platform, to build a library to collect roofline metrics of your application, and finally a script to present the results.

  The tool is able to benchmarks severals types of micro-operations: mul, add, fma, mad, load, load_nt, store, store_nt, 2ld1st, copy, explained later.

![](pictures/roofline_chart.png?raw=true)

This plot shows load instructions' rooflines (lines) and validation kernels (points) hitting the measured bandwidth.

###Set Up
```
git clone https://github.com/NicolasDenoyelle/LARM-Locality-Aware-Roofline-Model-.git
cd LARM-Locality-Aware-Roofline-Model-/src
make
```

Several options can be set in [Makefile](./src/Makefile)
* `LAST=` repeat the benchmark to make it last LAST milliseconds. This is useful in case you want to sample benchmarks performance with external sampling tool. Making longer benchmarks can give more stable results.

* `N_SAMPLES=`Number of sample to take for each memory benchmark. The samples scale is exponential, and bounds are automatically found according to memory sizes.

* `PAPI=no` Change to yes to compile librfsampling. This library allows to use functions defined in [sampling.h](src/sampling.h),
in order to output performance results from an application code, drawable on the roofline chart.

* `OMP_FLAG=` Change to your compiler openmp flag to enable parallel benchmark.


###Requirements

* This soft requires at least a recent enough hwloc library to be installed.
* The library using hardware counters uses papi, but it is not compiled by default.
* For now, it only works with intel processors but one can implement the interface MSC.h with other architectures code.
Be aware that though an optional [generic implementation](LARM-Locality-Aware-Roofline-Model-/blob/master/src/MSC/generic.c) (not used by default) is present, peak floating point benchmarks are not reliable. Achieving peak
performance with compiled code is a very hard task performed by very few finely tuned codes such as cblas_dgemm and can hardly be
achieved in a generic way.
* A gcc compiler on the machine running the validation benchmarks (`roofline -v`). Validation micro benchmarks are built, compiled and run at runtime. 
* R to use the [plot script](LARM-Locality-Aware-Roofline-Model-/blob/master/utils/plot_roofs.bash).

#### /!\ Important: 
Generally speaking, if you want to get relevant results on such benchmarks, you have to assert that options like turbo-boost are disabled and
the cpu frequency is set.
Therefore, it is required to export the variable CPU_FREQ (The frequency of your CPU in Hertz)
```
export CPU_FREQ=2100000000
```
Here you are ready to play

###Usage

* Display usage: `./roofline -h`

* Running benchmark: `./roofline`

* Running benchmark with validation `./roofline -v`

Validation consists in writing a list of load/store operations, interleaved with mul/add operations depending on the required operational intensity,
compile and run each benchmark.

* Run benchmark for precise types of micro operation: `./roofline -t "fma|load|store"`
  * fma: use fuse multiply add instructions for fpeak benchmarks (if architecture is capable of it)
  * mul: use multiply instructions for fpeak benchmarks.
  * add: use addition instructions for fpeak benchmarks.
  * mad: use interleaved additions/multiply instructions for fpeak benchmarks.
  * load: use load instructions for bandwidth benchmarks.
  * store: use store instructions for bandwidth benchmarks.
  * 2ld1st: Interleave one store each two loads.
  On some architectures, there are two loads channels on L1 cache and one store channel that can be used simulateously.
  Using those simultaneously may yields better bandwidth.
  * load_nt: use streaming load instructions for bandwidth benchmarks.
  The streaming instructions gives usually better memory bandwidth above caches because they bypass caches.
  It does not make much sense to use them for caches though you can.
  In case you see better results with streaming instructions instead of regular instructions on caches, they are rather due to measures variation than better hardware efficiency.
  * store_nt: use streaming store instructions for bandwidth benchmarks.
  
* plot help: `./utils/plot_roofs.bash -h`

* plot output: `./utils/plot_roofs.bash -i input -b`

###Library
If you compile the package with `PAPI=yes`, then you probably to get CARM metrics from your application.

####Library Requirements
* To compile the roofline library you have to install the PAPI library.
* The roofline library works only on broadwell processors with FP_ARITH counters. Older processor from Intel have no existing or accurate flops counter. Recently, PAPI KNL compatible version was released but no flops counters are yet available.

####Library Setup
* In order to use the library you have to copy the compiled file `librfsampling.so` in a directory pointed out by `LD_LIBRARY_PATH variable` or by your compiler linker flag (`-Lpath/to/librfsampling.so`). 
* You also have to copy the file `sampling.h` in a directory pointed out by `C_INCLUDE_PATH` or by your compiler header flag (`-Ipath/to/sampling.h`).
* The library is very lightweight and all the functions can be found in the header `sampling.h`
* Compile the code using the library with `-lrfsampling` flag

####Library example
```
#include <sampling.h>

roofline_sampling_init("my_CARM_result.roofs");
struct roofline_sample * result = new_roofline_sample(TYPE_LOAD);

roofline_sampling_start(result);
...
/* The code to evaluate */
...
roofline_sampling_stop(result);
roofline_sample_print (result);

delete_roofline_sample(result);
roofline_sampling_fini()
```

Then plot the results in a handsome chart:
`plot_roofs.bash -i plateform.roofs -d my_CARM_result.roofs -f "load|MAD|MUL|ADD" -t "CARM with my app" -o my_app_chart.pdf`

![](pictures/ddot.png?raw=true)

