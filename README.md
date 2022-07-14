# Artifact of "Distributed Continuation Stealing is More Scalable than You Might Think" in Cluster '22

This is the artifact of the following paper:

Shumpei Shiina, Kenjiro Taura. **Distributed Continuation Stealing is More Scalable than You Might Think.** in the 24th IEEE International Conference on Cluster Computing (Cluster ‘22). 2022.

## Directory Structure

- `massivethreads-dm`: MassiveThreads/DM
    - Original repository: https://github.com/shigeki-akiyama/massivethreads-dm
- `dnpbenchs`: Benchmarks for MassiveThreads/DM
- `isola`: job management tool
    - Original repository: https://github.com/s417-lama/isola
- `raw_results`: raw experimental data obtained in our environments

Competitors of UTS:
- `saws`: SAWS
    - Original repository: https://github.com/brianlarkins/saws
- `charm`: Charm++
    - Original repository: https://github.com/UIUC-PPL/charm
- `x10`: X10
    - Original repository: https://github.com/x10-lang/x10
    - The UTS benchmark was taken from: https://github.com/x10-lang/x10-benchmarks

### Subdirs in `massivethreads-dm`

Several versions of MassiveThreads/DM are separately managed in subdirs.

Continuation stealing with different join implementations (Section III):
- `waitq`: stalling join (baseline)
- `waitq_gc`: stalling join + local collection
- `greedy`: greedy join
- `greedy_gc`: greedy join + local collection

Child stealing (Section IV.B):
- `child_stealing`: *RtC* threads
- `child_stealing_ult`: *Full* threads

Extended *future* implementations for evaluation of LCS (Section V.D):
- `future_multi`: Cont. Steal (greedy) in TABLE III (based on `greedy_gc`)
- `waitq_gc_multi`: Cont. Steal (stalling) in TABLE III (based on `waitq_gc`)
- `child_stealing_ult_multi`: Child Steal (Full) in TABLE III (based on `child_stealing_ult`)

## Setup

### 1. Install Isola

In this artifact, we use a tool called `isola` to manage experimental environments for MassiveThreads/DM.

To install isola, run:
```sh
cd isola/
./install.sh # or `ISOLA_ROOT=/path/to/install ./install.bash` to explicitly set the installation path
```

and set the PATH env val (e.g., in `.bashrc`):
```sh
export PATH=~/.isola/bin:$PATH
```

To check if isola is correctly installed:
```sh
$ isola -h
Usage: isola [OPTIONS] COMMAND

COMMANDs:
  create    create an isola
  run       run commands in a temporal isola
  ls        list isolas
  rm        remove isolas
  where     show path to an isola dir
  cleanup   try to fix inconsistent state

OPTIONS:
  -h        show help
```

Please see [isola/README.md](./isola/README.md) for more information.

### 2. Install MPI

As MassiveThreads/DM uses MPI-3 RMA as a backend communication library, MPI needs to be installed.

Currently, we only tested the following MPI implementations and platforms:
- Open MPI v5.0.x for systems with Intel x86_64 CPUs and InfiniBand
- Fujitsu MPI for Fugaku-like systems (A64FX + Tofu Interconnect-D)

Important notes on the use of MPI-3 RMA:
- Open MPI v4 (or earlier) does not support MCA parameter `osc_ucx_acc_single_intrinsic`, which is crucial for work-stealing performance in MassiveThreads/DM.
  Thus, unless Open MPI v5.0.x (or newer) is used, the performance will be poor.
- Although many MPI implementations support one-sided communication (MPI-3 RMA), but some of them are not truly "one-sided"; progress of RMA operations depends on the remote side (i.e., unless the remote process calls some MPI functions, no progress is made for RMA operations).
    - See also the work by Schuchart et al. at ExaMPI '19: [Using MPI-3 RMA for Active Messages](https://sc19.supercomputing.org/proceedings/workshops/workshop_files/ws_exampi104s2-file1.pdf)
    - Open MPI and Fujitsu MPI (which is also based on Open MPI) are truly one-sided, so we used them for evaluation

In the following, we show how to install Open MPI v5.0.0rc7 with UCX v1.11.0, but the way of installation is not limited to this example.

Example script for UCX v1.11.0 installation:
```sh
UCX_VERSION=1.11.0
wget https://github.com/openucx/ucx/releases/download/v${UCX_VERSION}/ucx-${UCX_VERSION}.tar.gz
tar xvf ucx-${UCX_VERSION}.tar.gz
cd ucx-${UCX_VERSION}/
./contrib/configure-release --prefix=$UCX_INSTALL_PREFIX
make -j
make install
```

Example script for Open MPI v5.0.0rc7 installation:
```sh
wget https://download.open-mpi.org/release/open-mpi/v5.0/openmpi-5.0.0rc7.tar.gz
tar xvf openmpi-5.0.0rc7.tar.gz
cd openmpi-5.0.0rc7/
../configure --prefix=$OMPI_INSTALL_PREFIX --with-ucx=$UCX_INSTALL_PREFIX --disable-man-pages --with-pmix=internal --with-hwloc=internal --with-libevent=internal
make -j
make install
```

### 3. Setup Your Environment Files

`envs.sh` is loaded internally in many script files in this artifact; you should correctly configure this script for your system.

To configure `envs.sh`, add some branches to the if-else statements so that `$(hostname)` correctly matches your machine's hostname.

Variables to be set in `envs.sh`:
- `MACHINE_NAME`: Unique name for your machine
    - Used in measurement scripts in `dnpbenchs`
- `CPU_SIMD_FREQ`: CPU frequency while SIMD instructions are executed on all the cores (in GHz)
    - Used in synthetic benchmarks to set expected execution times for leaf tasks
    - SIMD instructions: AVX2 instructions for x86_64 and SVE instructions for A64FX
        - Note: the AVX frequency for Intel CPUs is often different from the base CPU frequency

Additionally, you can load some PATH variables (e.g., paths to compilers, MPI) in `envs.sh` (not mandatory).

### 4. Build MassiveThreads/DM

`massivethreads-dm/make_common.bash` is a common script to build each version of MassiveThreads/DM.
You can modify this script to configure MassiveThreads/DM for each machine.

The following configure command will work for most cases:
```sh
CCFLAGS=$CFLAGS CXXFLAGS=$CFLAGS ./configure --with-comm-layer=mpi3 ${logger_options[@]+"${logger_options[@]}"} --prefix=${INSTALL_DIR}
```

- `--with-comm-layer=mpi3` is mandatory; other communication layers do not work.
- `--enable-polling` option is for MPI-3 RMA implementations that are not truly one-sided; it inserts polling MPI calls (`MPI_Iprobe()`) to the work-stealing scheduler's code, which might add significant overheads to fine-grained tasks
    - If the execution hangs, try this option

To build all versions of MassiveThreads/DM, run:
```sh
cd massivethreads-dm/make_common.bash
./make_all.bash
```

All versions of MassiveThreads/DM are installed by Isola.
You can check your installations:
```sh
$ isola ls massivethreads-dm
massivethreads-dm:child_stealing
massivethreads-dm:child_stealing_prof
massivethreads-dm:child_stealing_ult
massivethreads-dm:child_stealing_ult_multi
massivethreads-dm:child_stealing_ult_prof
massivethreads-dm:child_stealing_ult_trace
massivethreads-dm:future_multi
massivethreads-dm:greedy
massivethreads-dm:greedy_gc
massivethreads-dm:greedy_gc_prof
massivethreads-dm:greedy_gc_trace
massivethreads-dm:waitq
massivethreads-dm:waitq_gc
massivethreads-dm:waitq_gc_multi
massivethreads-dm:waitq_gc_prof
massivethreads-dm:waitq_gc_trace
massivethreads-dm:waitq_prof
```

Isola names `*_prof` are profiling-enabled versions, and `*_trace` are tracing-enabled versions for timeline visualization.

### 5. Configure Experiments

Before running experiments, you need to configure the `dnpbenchs/measure_jobs/common.bash` file.

In the `dnp_run_common()` function in that file, you need to set the following three parameters:
- `DNP_CORES` variable: the maximum number of cores within a node in your system.
- `DNP_NODES` variable: the number of nodes allocated by the job manager (e.g., slurm)
    - e.g.,, `SLURM_JOB_NODES` in slurm
    - If you do not use any job manager, manually set this variable for each measurement
- `dnp_mpirun()` function: specify how to invoke an MPI program (see below)

An example of `dnp_mpirun()` in Open MPI:
```sh
dnp_mpirun() {
  local n_processes=$1
  local n_processes_per_node=$2
  local out_file=$3
  mpirun -n $n_processes -N $n_processes_per_node \
    --mca osc_ucx_acc_single_intrinsic true \
    ../opt/massivethreads-dm/bin/madm_disable_aslr "${@:4}" | tee $out_file
}
```

In addition, you may want to specify where the hostfile exists and some other options.

Note: The `--mca osc_ucx_acc_single_intrinsic true` option is critical to the performance as mentioned eariler.

## Run Experiments

### Synthetic Benchmarks (Section V.A and V.B)

Files:
- `dnpbenchs/sched_bench.{cpp,hpp}`: C++ code for PFor benchmark and RecPFor benchmark (Section IV.C)
- `dnpbenchs/measure_jobs/sched_pfor.bash`: measurement config for PFor
- `dnpbenchs/measure_jobs/sched_rrm.bash`: measurement config for RecPFor
- `dnpbenchs/scripts/batch_sched.bash`: batch script to run measurement jobs sequentially
- (optional) `dnpbenchs/scripts/batch_sched_submit.bash`: batch script to run measurement jobs in parallel by using a job management system
    - You need to configure `dnpbenchs/scripts/submit.bash` by specifying how to submit a job to the job management system in your environment

To run:
```sh
cd dnpbenchs
./scripts/batch_sched.bash
```

After running experiments, the results will be saved in each Isola dir.

List of Isola dirs of dnpbenchs:
```sh
$ isola ls dnpbenchs
dnpbenchs:sched_pfor_size_n_no_buf_madm_child_stealing_prof_node_1
dnpbenchs:sched_pfor_size_n_no_buf_madm_child_stealing_ult_prof_node_1
dnpbenchs:sched_pfor_size_n_no_buf_madm_greedy_gc_prof_node_1
dnpbenchs:sched_pfor_size_n_no_buf_madm_waitq_gc_prof_node_1
dnpbenchs:sched_pfor_size_n_no_buf_madm_waitq_prof_node_1
dnpbenchs:sched_rrm_size_n_no_buf_madm_child_stealing_prof_node_1
dnpbenchs:sched_rrm_size_n_no_buf_madm_child_stealing_ult_prof_node_1
dnpbenchs:sched_rrm_size_n_no_buf_madm_greedy_gc_prof_node_1
dnpbenchs:sched_rrm_size_n_no_buf_madm_waitq_gc_prof_node_1
dnpbenchs:sched_rrm_size_n_no_buf_madm_waitq_prof_node_1
```

The results are stored as a sqlite DB file in each isola.

To check the DB file:
```sh
$ sqlite3 $(isola where dnpbenchs:sched_pfor_size_n_no_buf_madm_greedy_gc_prof_node_1)/dnpbenchs/result.db
sqlite> select * from dnpbenchs;
...
```

The raw result files are also available:
```sh
$ ls $(isola where dnpbenchs:sched_pfor_size_n_no_buf_madm_greedy_gc_prof_node_1)/dnpbenchs/out
node_1_n_1048576_m_10000_i_1.txt  node_1_n_32768_m_10000_i_1.txt
node_1_n_131072_m_10000_i_1.txt   node_1_n_4096_m_10000_i_1.txt
node_1_n_16384_m_10000_i_1.txt    node_1_n_524288_m_10000_i_1.txt
node_1_n_2097152_m_10000_i_1.txt  node_1_n_65536_m_10000_i_1.txt
node_1_n_262144_m_10000_i_1.txt   node_1_n_8192_m_10000_i_1.txt
```

Variables in `dnpbenchs/scripts/batch_sched.bash`:
- `REPEAT_OUTSIDE`: the number of repetitions outside each program invocation
- `REPEAT_INSIDE`: the number of repetitions inside each program invocation
    - Initially set to 3; to get more stable results, please increase this value (e.g., 100)
- `DNP_MAX_NODES`: the number of nodes to be used

#### Plot Results

First, you need to modify plotting scripts `dnpbenchs/plot/sched_bench_size_n.py` and `dnpbenchs/plot/sched_bench_prof.py`.

Variables to be set:
- `machine`: the machine name (which you set in `envs.bash`)
- `nodes`: the number of nodes you used to run benchmarks

To get the plot, run:
```sh
cd dnpbenchs/plot
python3 sched_bench_size_n.py
python3 sched_bench_prof.py
```

It will collect results from isola dirs and output the plot .html files in `dnpbenchs/plot`.
You can open them in your browser.

Note: if `CPU_SIMD_FREQ` is not properly configured, the parallel efficiency will be incorrect.

### UTS (Section V.C)

#### Run UTS with MassiveThreads/DM

Files:
- `dnpbenchs/uts/*`: UTS benchmark code
- `dnpbenchs/measure_jobs/uts.bash`: measurement config for UTS
- `dnpbenchs/scripts/batch_uts_submit.bash`: batch script to run measurement jobs in parallel by using a job management system
    - need to be manually configured to specify which tree sizes and how many nodes to be used
    - You need to configure `dnpbenchs/scripts/submit.bash` by specifying how to submit a job to the job management system in your environment
        - currently it is mandatory for UTS
        - If you feel it is burdensome to configure it, you can manually check the performance of UTS, as described in the "Play with Benchmarks" section below

To submit jobs for UTS with MassiveThreads/DM:
```sh
cd dnpbenchs
./scripts/batch_uts_submit.bash <your_machine_name>
```

#### Run UTS with `saws`, `charm`, and `x10`

Some notes:
- All of these UTS implementations were modified from the original, so that experimental conditions match our settings
    - e.g., to run for multiple times within a single program invocation
- These experiments do not require Isola

Files in each dir:
- `make.bash`: script to compile runtimes and UTS
    - need to be configured to load environment for compilation
- `run_uts.bash`: script to run UTS
    - need to be configured to specify how to invoke `mpirun` command
- `submit.bash`: script to submit a job to the system job manager
    - need to be configured to tell how jobs should be submitted to the system job manager
- `submit_batch.bash`: batch script to submit multiple jobs
    - need to be manually configured to specify which tree sizes and how many nodes to be used
    - to get more stable results, increase `REPEATS` value
- `collect_result.bash`: collect the results as `result.db`

NOTE: the above files do not load environment from `envs.bash`; you need to manually configure each of the above files.

Run the following commands to compile, submit, and collect the results in each directory:
```sh
cd <runtime>
./make.bash
./submit_batch.bash
# after completion of all jobs
./collect_result.bash
```

where `<runtime>` is either `saws`, `charm`, or `x10`.

You can see the collected results using sqlite3:
```
$ sqlite3 <runtime>/result.db
sqlite> select * from <runtime>_uts;
...
```

#### Plot Results

To get the plot, run:
```sh
cd dnpbenchs/plot
python3 uts.py
```

Variables:
- `n_runs`: the number of repetitions
    - it needs to be properly set, because only the last `n_runs` runs are used for plotting (to exclude warm-up runs)
- `get_n_cores_per_node()` function: the number of cores per node for each machine

### LCS (Section V.D)

Files:
- `dnpbenchs/lcs.cpp`: LCS benchmark code
- `dnpbenchs/measure_jobs/lcs.bash`: measurement config for LCS
- `dnpbenchs/scripts/batch_lcs_submit.bash`: batch script to run measurement jobs in parallel by using a job management system
    - see the decsription of UTS

To submit jobs:
```sh
cd dnpbenchs
./scripts/batch_lcs_submit.bash <your_machine_name>
```

For `waitq_gc_multi` and `child_stealing_ult_multi` (TABLE III), please run the LCS benchmark manually by consulting the "Play with Benchmarks" section below.

#### Plot Results

To get the plot, run:
```sh
cd dnpbenchs/plot
python3 lcs.py
```

Variables:
- `n_runs`: the number of repetitions
    - it needs to be properly set, because only the last `n_runs` runs are used for plotting (to exclude warm-up runs)
- `get_n_cores_per_node()` function: the number of cores per node for each machine

## Play with Benchmarks

The `dnpbenchs/run_jobs` dir contains script files to run each benchmark program.
Please configure the `dnpbenchs/run_jobs/common.bash` file similarly to `dnpbenchs/measure_jobs/common.bash`

To run the synthetic benchmark with `greedy_gc` version of MassiveThreads:
```sh
cd dnpbenchs
isola run -b massivethreads-dm:greedy_gc ./run_jobs/sched_bench.bash
```

The output will look like:
```
...
## Running on 1 nodes


### N = 131072, M = 10000
    Work:         6,553,600,000 ns
    Span:                50,000 ns
    Ideal:          182,044,444 ns

MADM_COMM_ALLOCATOR_INIT_SIZE = 2097152
MADM_FUTURE_POOL_BUF_SIZE = 131072
=============================================================
[pfor]
# of processes:                36
N (Input size):                131072
M (Leaf execution time in ns): 10000
# of blocks:                   5
# of repeats:                  10
# of warmup runs:              1000
CPU frequency:                 3.300000 GHz
# of loop iterations at leaf:  4125
-------------------------------------------------------------
uth options:
MADM_DEBUG_LEVEL = 0, MADM_DEBUG_LEVEL_RT = 5, MADM_CORES = 16, MADM_SERVER_MOD = 0, MADM_GASNET_POLL_THREAD = 0, MADM_GASNET_SEGMENT_SIZE = 0
MADM_STACK_SIZE = 1048576, MADM_TASKQ_CAPACITY = 1024, MADM_PROFILE = 0, MADM_STEAL_LOG = 0, MADM_ABORTING_STEAL = 1
=============================================================

warmup time: 2158617590 ns
[0] 189445692 ns ( #tasks: 655360 work: 6632153500 ns span: 3335224 ns busy: 97.245012 % )
[1] 189493342 ns ( #tasks: 655360 work: 6628007344 ns span: 161585 ns busy: 97.159780 % )
[2] 189273412 ns ( #tasks: 655360 work: 6625861341 ns span: 268257 ns busy: 97.241182 % )
[3] 189092104 ns ( #tasks: 655360 work: 6627959055 ns span: 1926540 ns busy: 97.365236 % )
[4] 189366113 ns ( #tasks: 655360 work: 6626216833 ns span: 228027 ns busy: 97.198794 % )
[5] 189167448 ns ( #tasks: 655360 work: 6626494767 ns span: 405674 ns busy: 97.304954 % )
[6] 189203665 ns ( #tasks: 655360 work: 6625076636 ns span: 97958 ns busy: 97.265508 % )
[7] 188743016 ns ( #tasks: 655360 work: 6625011176 ns span: 132042 ns busy: 97.501933 % )
[8] 189261201 ns ( #tasks: 655360 work: 6624877476 ns span: 205415 ns busy: 97.233016 % )
[9] 188965016 ns ( #tasks: 655360 work: 6624631946 ns span: 157376 ns busy: 97.381811 % )
```

Notes:
- `uth options` are obsolete.
- For each result line:
    - The execution time is shown in nanoseconds
    - `#tasks`: the number of leaf tasks
    - `work`: measured total work
    - `span`: measured span (critical path length)
    - `busy`: busyness of workers (`work / (execution time) / (# of processes)`)
- `M (Leaf execution time in ns)` should be (almost) equal to `work / #tasks`.
    - e.g., for 0th iteration in the above case, `6632153500 / 655360 = 10120`, which is close to `M=10000`
    - If not, environment variable `CPU_SIMD_FREQ` is incorrect

You can configure benchmark settings in `dnpbenchs/run_jobs/sched_bench.bash`; for example, by commenting out `compute_pattern=rrm`, you can run RecPFor benchmark.

## Generate Time Series Plots

By default, three builds `greedy_gc_trace`, `waitq_gc_trace`, and `child_stealing_ult_trace` are available for tracing.

To trace the execution of synthetic benchmarks, you need to set `prof=1` in `dnpbenchs/run_jobs/sched_bench.bash`.

An example to trace execution with `greedy_gc_trace`:
```sh
cd dnpbenchs
isola create -b massivethreads-dm:greedy_gc_trace -o test ./run_jobs/sched_bench.bash
```

The trace output will be saved in isola `dnpbenchs:test`.

Then, you can visualize the timeline (like Fig. 7) by running:
```sh
cd dnpbenchs/plot
python3 timeline.py
```

(it will take a lot of time if the problem size is large)

Additional notes:
- The output `mlog.ignore` can be viewed as a timeline (gantt chart) by using [massivelogger](https://github.com/massivethreads/massivelogger) viewer
- Other events are also available; you can add more events to `enabled_kinds` in `massivethreads-dm/make_common.bash`
    - list of events: `massivethreads-dm/greedy_gc/comm/include/madm_logger_kind.h`

## Check Raw Results in Our Environments

The `raw_results` dir contains raw data and generated figures used for the paper.

Subdirs:
- `raw_results/ito-a`
    - ITO supercomputer (subsystem A) at Kyushu University
- `raw_results/wisteria-o`
    - Wisteria/BDEC-01 Odyssey at The University of Tokyo

`viewer.py` will launch a convenient viewer for the result HTML files:
```sh
cd raw_results/ito-a
python3 viewer.py
```

Each subdir also includes python scripts to generate plots from raw data.

For example, if you want to generate plots for UTS on ITO-A, run:
```sh
cd raw_results/ito-a
python3 uts.py
```
