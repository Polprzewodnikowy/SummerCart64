#!/usr/bin/env python3
"""Benchmark per-file SD uploads against the batched `sd sync` command."""

import argparse
import hashlib
import json
import pathlib
import statistics
import subprocess
import sys
import tempfile
import time
import uuid


def run(deployer, port, *args, capture=False, check=True):
    command = [str(deployer), "--port", port, *map(str, args)]
    return subprocess.run(
        command,
        check=check,
        capture_output=capture,
        text=True,
    )


def remote_path(root, relative):
    return f"{root}/{relative.as_posix()}"


def remove_tree(deployer, port, root, files, directories):
    for relative in reversed(files):
        run(deployer, port, "sd", "rm", remote_path(root, relative), capture=True, check=False)
    for relative in sorted(directories, key=lambda path: len(path.parts), reverse=True):
        run(deployer, port, "sd", "rm", remote_path(root, relative), capture=True, check=False)
    run(deployer, port, "sd", "rm", root, capture=True, check=False)


def remote_exists(deployer, port, root):
    result = run(deployer, port, "sd", "stat", root, capture=True, check=False)
    return result.returncode == 0


def hash_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def verify_tree(deployer, port, source, root, files, download_root):
    for relative in files:
        downloaded = download_root / relative
        downloaded.parent.mkdir(parents=True, exist_ok=True)
        run(deployer, port, "sd", "download", remote_path(root, relative), downloaded)
        if hash_file(source / relative) != hash_file(downloaded):
            raise RuntimeError(f"SHA-256 mismatch: {relative}")


def benchmark_baseline(deployer, port, source, root, files, directories):
    started = time.perf_counter()
    run(deployer, port, "sd", "mkdir", root)
    for relative in sorted(directories, key=lambda path: len(path.parts)):
        run(deployer, port, "sd", "mkdir", remote_path(root, relative))
    for relative in files:
        run(deployer, port, "sd", "upload", source / relative, remote_path(root, relative))
    return time.perf_counter() - started


def benchmark_batched(deployer, port, source, root):
    started = time.perf_counter()
    run(deployer, port, "sd", "sync", source, root)
    return time.perf_counter() - started


def summarize(values, byte_count):
    median = statistics.median(values)
    return {
        "min_seconds": min(values),
        "median_seconds": median,
        "mean_seconds": statistics.mean(values),
        "max_seconds": max(values),
        "median_kib_per_second": byte_count / 1024 / median,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, type=pathlib.Path)
    parser.add_argument("--candidate", required=True, type=pathlib.Path)
    parser.add_argument("--port", required=True, help="For example: serial://COM5")
    parser.add_argument("--source", required=True, type=pathlib.Path)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()

    if args.runs < 1:
        parser.error("--runs must be at least 1")
    for executable in (args.baseline, args.candidate):
        if not executable.is_file():
            parser.error(f"deployer not found: {executable}")
    source = args.source.resolve()
    if not source.is_dir():
        parser.error(f"source is not a directory: {source}")

    info = run(args.candidate, args.port, "info", capture=True).stdout
    if "Current CIC step:  Power off" not in info:
        raise RuntimeError("console must be powered off before benchmarking SD access")

    files = sorted(path.relative_to(source) for path in source.rglob("*") if path.is_file())
    if not files:
        raise RuntimeError("source tree has no files")
    directories = {parent for path in files for parent in path.parents if parent != pathlib.Path(".")}
    byte_count = sum((source / path).stat().st_size for path in files)
    session = uuid.uuid4().hex[:12]
    results = {"baseline": [], "batched": []}
    roots = []

    with tempfile.TemporaryDirectory(prefix="sc64-sd-sync-benchmark-") as temporary:
        downloads = pathlib.Path(temporary)
        try:
            for run_index in range(args.runs):
                order = ("baseline", "batched") if run_index % 2 == 0 else ("batched", "baseline")
                for method in order:
                    root = f"/sc64-sync-benchmark-{session}-{method}-{run_index + 1}"
                    if remote_exists(args.candidate, args.port, root):
                        raise RuntimeError(f"refusing to reuse existing remote path: {root}")
                    roots.append(root)
                    if method == "baseline":
                        elapsed = benchmark_baseline(
                            args.baseline, args.port, source, root, files, directories
                        )
                    else:
                        elapsed = benchmark_batched(args.candidate, args.port, source, root)
                    verify_tree(
                        args.candidate,
                        args.port,
                        source,
                        root,
                        files,
                        downloads / f"{method}-{run_index + 1}",
                    )
                    results[method].append(elapsed)
                    print(f"{method} run {run_index + 1}: {elapsed:.3f} s", flush=True)
        finally:
            for root in roots:
                remove_tree(args.candidate, args.port, root, files, directories)

    baseline = summarize(results["baseline"], byte_count)
    batched = summarize(results["batched"], byte_count)
    report = {
        "port": args.port,
        "source": str(source),
        "runs": args.runs,
        "files": len(files),
        "directories": len(directories),
        "bytes": byte_count,
        "baseline": baseline,
        "batched": batched,
        "median_speedup": baseline["median_seconds"] / batched["median_seconds"],
        "median_time_reduction_percent": (
            1 - batched["median_seconds"] / baseline["median_seconds"]
        )
        * 100,
    }
    rendered = json.dumps(report, indent=2)
    print(rendered)
    if args.output:
        args.output.write_text(rendered + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
