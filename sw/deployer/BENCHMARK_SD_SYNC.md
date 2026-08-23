# SD sync physical benchmark

`benchmark_sd_sync.py` compares the existing one-process-per-file workflow with
the batched `sd sync` command on a physical SummerCart64.

Requirements:

- Python 3.8 or newer
- a baseline `sc64deployer` without `sd sync`
- a candidate `sc64deployer` built from this branch
- a powered-off console
- a source directory whose contents may be copied to temporary SD paths

Example:

```console
python sw/deployer/benchmark_sd_sync.py \
  --baseline /path/to/released/sc64deployer \
  --candidate /path/to/patched/sc64deployer \
  --port serial://COM5 \
  --source sw/deployer/src \
  --runs 5 \
  --output benchmark.json
```

The script alternates method order, creates a unique remote path for every run,
downloads and SHA-256 verifies every uploaded file, and attempts to remove all
benchmark paths even when a run fails. It refuses to run unless the deployer
reports that the console is powered off.
