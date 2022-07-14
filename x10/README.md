# x10

This repository contains the core implementation of X10 programming
language including compiler, runtime, class libraries, sample programs
and test suite.  For more information about X10, including how to
build the X10 toolchain from the source files in this repository,
please see http://x10-lang.org.

[![Build Status - Master](https://travis-ci.org/x10-lang/x10.svg?branch=master)](https://travis-ci.org/x10-lang/x10)
[![DOI](https://zenodo.org/badge/21876/x10-lang/x10.svg)](https://zenodo.org/badge/latestdoi/21876/x10-lang/x10)

## How to run UTS

`GLB-UTS` dir was copied from [x10-benchmarks](https://github.com/x10-lang/x10-benchmarks).

A few modifications are made (e.g., repeat the execution within the program).

Build:
```
./make.bash
```

Run:
```
./run_uts.bash
```

Submit `run_uts.bash` as a job:
```
./submit.bash
```

Submit multiple jobs at a time:
```
./submit_batch.bash
```

Collect the results stored in `GLB-UTS/out` as `result.db`:
```
./collect_result.bash
```
