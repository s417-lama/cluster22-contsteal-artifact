# saws
OpenSHMEM accelerated work stealing

## How to run

(added by Shumpei Shiina)

The original project was modified to run for multiple times to include warmup runs.
Some convenient scripts for performance measurement were also added.

Build:
```
./make.bash
```

It is recommended to run `make.bash` on the compute server.

Run UTS benchmark:
```
./run_uts.bash
```

There are many parameters defined in `run_uts.bash`.
You can modify them directly or via environment variables (as `submit_batch.bash` does).

Submit a job (`run_uts.bash`) to the job management system:
```
./submit.bash
```

Currently it supports only supercomputer ITO (my setting).
It is needed to modify `submit.bash` when running on other platforms.

Submit many jobs with settings for performance evaluation:
```
./submit_batch.bash
```

Settings are given as environment variables and passed to `run_uts.bash`.
It internally executes `submit.bash` for many times.

Collect the results into a sqlite database file (`result.db`):
```
./collect_result.bash
```

`result.db` is generated.
It uses `a2sql` for collecting raw outputs in `examples/uts/out`.

## License

This project (c) 2021 D. Brian Larkins
