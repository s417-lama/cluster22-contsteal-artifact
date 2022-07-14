# DNPBenchs

Distributed Nested Parallel Benchmarks

## Getting started

In massivethreads-dm:
```
isola create -o mpi3 ./make.bash
```

In dnpbenchs:
```
isola run -b massivethreads-dm:mpi3 ./run_jobs/hello.bash
```
