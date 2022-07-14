# A Tutorial on Managing Multiple Builds of OpenBLAS

This tutorial will create two versions of OpenBLAS builds for different architectures.
Suppose that you have two machines in your environment, which are managed by Slurm:

- `spica` : 96 core Intel Haswell architecture
- `betelgeuse` : 64 core AMD Zen architecture

Please replace these settings with ones in your environment.

## Build OpenBLAS in an isola

Suppose that the current dir is `tutorial/openblas`.

Clone the OpenBLAS git repo of version 0.3.10.
```sh
$ git clone https://github.com/xianyi/OpenBLAS.git -b v0.3.10
$ cd OpenBLAS
```

Let's build OpenBLAS for both Haswell and Zen architectures.

```sh
## Create an isola named 'haswell' for 96 core Intel Haswell architecture (spica)
$ isola create haswell bash -c 'make ONLY_CBLAS=1 TARGET=HASWELL NUM_THREADS=96 && make install PREFIX=${ISOLA_HOME}/opt/openblas'

## Create an isola named 'zen' for 64 core AMD Zen architecture (betelgeuse)
$ isola create zen bash -c 'make ONLY_CBLAS=1 TARGET=ZEN NUM_THREADS=64 && make install PREFIX=${ISOLA_HOME}/opt/openblas'
```

`isola create` command creates a new isola with the copy of the current dir and runs commands (`make -c '...'` here) on the isola.
`$ISOLA_HOME` environment variable holds a path to the virtualized HOME directory and is automatically set within the isola.
With the above make commands, OpenBLAS is installed at `${ISOLA_HOME}/opt/openblas`.

The resulting directory structure under `$ISOLA_HOME` will be like this:

```
$ISOLA_HOME
├── OpenBLAS
│   └── ...
└── opt
    └── openblas
        ├── bin
        ├── include
        └── lib
```

Check if isolas are successfully created by `isola ls` command.

```sh
$ isola ls -r OpenBLAS
OpenBLAS:haswell:2020-07-03_17-31-03
OpenBLAS:haswell:latest
OpenBLAS:zen:2020-07-03_17-29-16
OpenBLAS:zen:latest
```

It lists existing isola IDs.
The format of an isola ID is `<project>:<name>:<version>`, where
- `<project>` : the directory name where `isola create` is executed
- `<name>`    : the name specified in `isola create` command
- `<version>` : timestamp is automatically given. 'latest' is an alias to the latest version

## Run a program in an isola of OpenBLAS

First, move to `dgemm_example` dir.
```sh
$ cd ../dgemm_example
```

`dgemm_example` has the following files:

- dgemm.c --- a program that calls `cblas_dgemm` in OpenBLAS
- Makefile --- compiles dgemm.c
- run.sh --- a job script (make and run)

Makefile is *isola-aware*, that is, it refers to the install path of OpenBLAS by `$ISOLA_HOME` environment variable.
By using `$ISOLA_HOME` only when it exists, the Makefile works even without isola:

```
VHOME     := $(or $(ISOLA_HOME),$(HOME))
BLAS_PATH := $(VHOME)/opt/openblas
```

Then let's run the program in each isola.
After logging in to each compute server, run the following commands:

On `spica`:
```sh
$ isola run -b OpenBLAS:haswell ./run.sh
```
The current dir is copied to (the clone of) the isola `OpenBLAS:haswell`, and `./run.sh` is executed on the isola.

On `betelgeuse`:
```sh
$ isola run -b OpenBLAS:zen ./run.sh
```
The current dir is copied to (the clone of) the isola `OpenBLAS:zen`, and `./run.sh` is executed on the isola.

`isola run` command creates a disposable isola with the copy of the current dir and run commands on it.
The changes made in the isola will not be saved, keeping the base isola and the current dir unmodified.
The base isola can be specified with `-b <isola_id>` option.
If you omit the version, the 'latest' version is automatically selected.
Note that the base isola is also copied.

The directory structure will look like:
```
$ISOLA_HOME
├── OpenBLAS
│   └── ...
├── dgemm_example
│   ├── dgemm.c
│   ├── Makefile
│   └── run.sh
└── opt
    └── openblas
        ├── bin
        ├── include
        └── lib
```

## Incorporate with Slurm

Isola is easy to incorporate with job management systems in clusters or supercomputers, such as Slurm.
This tutorial section explains the benefits of using Isola with Slurm as an example.

### Run a program in a disposable directory

By combining `isola run` and `srun`, you can play with a program without saving any changes to the current dir.

```sh
## Create a disposable dir based on the isola 'OpenBLAS:haswell' and run commands on 'spica'
$ isola run -b OpenBLAS:haswell srun -w spica bash -c 'make && ./dgemm'

## Create a disposable dir based on the isola 'OpenBLAS:zen' and run commands on 'betelgeuse'
$ isola run -b OpenBLAS:zen srun -w betelgeuse bash -c 'make && ./dgemm'
```

`srun` is invoked within a newly-created disposable isola.

### Interactively run bash in an isola

You can also interactively run programs by launching bash.

```sh
$ isola run -b OpenBLAS:haswell srun -w spica --pty bash
$ isola run -b OpenBLAS:zen srun -w betelgeuse --pty bash
```

### Submit a job with `sbatch` command in an isola

You can submit multiple jobs in parallel and save the results by `isola create` and `sbatch`.

```sh
## Create a new isola named 'dgemm_example:spica' based on the isola 'OpenBLAS:haswell'
$ isola create -b OpenBLAS:haswell spica sbatch -w spica ./run.sh

## Create a new isola named 'dgemm_example:betelgeuse' based on the isola 'OpenBLAS:zen'
$ isola create -b OpenBLAS:zen betelgeuse sbatch -w betelgeuse ./run.sh
```

New isolas are created immediately.

```sh
$ isola ls -r dgemm_example
dgemm_example:betelgeuse:2020-07-03_17-34-27
dgemm_example:betelgeuse:latest
dgemm_example:spica:2020-07-03_17-34-24
dgemm_example:spica:latest
```

After the job completes, the output of Slurm (e.g., `slurm-<ID>.out`) is also written to each isola.
By following this procedure, you can avoid messing up a lot of output files generated by slurm or other job management systems.

The output files can be seen by `isola where` command.

```sh
## Show results on 'spica'
$ cat $(isola where dgemm_example:spica)/dgemm_example/slurm-*.out
cc -o dgemm dgemm.c -I~/.isola/projects/dgemm_example/betelgeuse/2020-07-03_17-34-27/opt/openblas/include -L~/.isola/projects/dgemm_example/betelgeuse/2020-07-03_17-34-27/opt/openblas/lib -Wl,-R~/.isola/projects/dgemm_example/betelgeuse/2020-07-03_17-34-27/opt/openblas/lib -lopenblas
0 : 1479697555 ns
1 : 1346526126 ns
2 : 1080780542 ns
3 : 1038688405 ns
4 : 1185552776 ns
5 : 1221602258 ns
6 : 1248776989 ns
7 : 1251177056 ns
8 : 1243041009 ns
9 : 1242917573 ns

## Show results on 'betelgeuse'
$ cat $(isola where dgemm_example:betelgeuse)/dgemm_example/slurm-*.out
cc -o dgemm dgemm.c -I~/.isola/projects/dgemm_example/betelgeuse/2020-07-03_17-34-27/opt/openblas/include -L~/.isola/projects/dgemm_example/betelgeuse/2020-07-03_17-34-27/opt/openblas/lib -Wl,-R~/.isola/projects/dgemm_example/betelgeuse/2020-07-03_17-34-27/opt/openblas/lib -lopenblas
0 : 2297111027 ns
1 : 2167922513 ns
2 : 2125846148 ns
3 : 2078450331 ns
4 : 2030998716 ns
5 : 2033538882 ns
6 : 2083787992 ns
7 : 2048352768 ns
8 : 2046179495 ns
9 : 1994614866 ns
```
