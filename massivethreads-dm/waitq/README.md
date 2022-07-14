# Uni-Address Threads

Forked from: https://github.com/shigeki-akiyama/massivethreads-dm

## How to Run Locally

Compile:
```
./configure
make
```

Run a benchmark program (Binary Task Creation; bin):
```
./misc/madmrun/madmrun -n <number of processes> uth/examples/bin/bin <depth> <leaf_loops> <interm_loops> <interm_iters pre_exec>
```

Parameters:
- `depth`: depth of the binary tree
- `leaf_loops`: how many clocks are consumed in leaf tasks (in clocks)
- `interm_loops`: how many clocks are consumed in intermediate tasks in the tree (in clocks)
- `interm_iters`: number of fork-join blocks in intermediate tasks
- `pre_exec`: number of warm-up runs

Example result:
```
$ ./misc/madmrun/madmrun -n 6 uth/examples/bin/bin 7 10000 1000 3 1
program = bin,
depth = 7, leaf_loops = 10000, interm_loops = 1000, interm_iters = 3, pre_exec = 1
MADM_DEBUG_LEVEL = 0, MADM_DEBUG_LEVEL_RT = 5, MADM_CORES = 2147483647, MADM_SERVER_MOD = 0, MADM_GASNET_POLL_THREAD = 0, MADM_GASNET_SEGMENT_SIZE = 0
MADM_STACK_SIZE = 1048576, MADM_TASKQ_CAPACITY = 1024, MADM_PROFILE = 0, MADM_STEAL_LOG = 0, MADM_ABORTING_STEAL = 1
np = 6, server_mod = 0, time = 0.230996,
throughput = 1.454238, throughput/np = 0.242373, task overhead = 4126
```

You may need to modify the option for setting an environment variable in `mpirun` in the script `misc/madmrun/madmrun`:

For OpenMPI:
```
-x MADM_RUN__=1
```

For MPICH:
```
-env MADM_RUN__ 1
```
