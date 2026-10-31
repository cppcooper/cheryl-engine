#!/usr/bin/env python3
"""Finite WORLD-U9 measurement; invoke through scripts/build -- python3 measure.py."""

import argparse
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path.cwd()
    binary = args.binary.resolve()
    output = args.output.resolve()
    lock = Path("/mnt/projects/cheryl-features/runtime/locks/build.lock")
    # The wrapper passes its locked descriptor to this serial test driver.
    # Retain it for the entire finite batch, as CTest does for a test selection.
    inherited_lock = any(Path(fd).resolve() == lock for fd in Path("/proc/self/fd").glob("*"))
    if not inherited_lock:
        raise RuntimeError("invoke this driver through /mnt/projects/cheryl-features/scripts/build --")
    output.mkdir(parents=True, exist_ok=False)
    cases = list(itertools.product(("list", "map"), ("ordered", "shuffled")))
    plan = {
        "version": 1, "seed": "0x9e3779b9", "groups": 16,
        "live_limit": 512, "allocations_per_trace": 1776,
        "traversal_repetitions_per_stage": 16, "stages": 7,
        "map_query_repetitions_per_stage": 16,
        "cold_processes_per_case_backend": 7, "warm_traces_per_process": 1,
        "clock_pair_probes_per_timing_process": 32,
        "observation_processes": 8, "timing_processes": 56,
        "max_native_invocations": 64, "admission_time_budget_seconds": 600,
        "native_queue": "One wrapper admission/lock for this serial test batch; subprocesses inherit its resource scope.",
        "deadline_policy": "Check between invocations; retain a live shared-lock wait; no retries.",
        "binary": str(binary), "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "quantiles": "nearest rank: sorted[ceil(q*n)-1]; with n=7 p95 is max",
        "timing_scope": "Per-phase steady_clock nanoseconds. Sum excludes between-phase work; not process wall time.",
        "exclusions": [
            "Separate observed allocator runs, owner/metadata snapshots, address reuse search, culling.",
            "Literal expected sequences, full pair validation, surviving-address checks, seed generation check.",
            "Process/DSO loading, stdout formatting, container/facade construction and final destruction outside clear.",
            "No cache counters, CPU affinity, privileged setup, global allocator interception or backend tuning.",
        ],
        "included_costs": [
            "Actual std or Engine allocator, Engine locks, shared owners, metadata reservations and lookup.",
            "Record construction/destruction, container operations, useful traversal/query reduction, clock calls.",
            "Map insert-success/hit/miss checks run equally on both backends.",
        ],
        "environment": {k: os.environ[k] for k in ("ASAN_OPTIONS", "UBSAN_OPTIONS") if k in os.environ},
    }
    (output / "plan.json").write_text(json.dumps(plan, indent=2) + "\n")
    started = time.monotonic()
    samples, results = [], {}

    def invoke(label, mode, kind, backend, order):
        if time.monotonic() - started > plan["admission_time_budget_seconds"]:
            raise RuntimeError("finite admission time budget exhausted; no further invocations")
        command = [str(binary), mode, kind, backend, order]
        begin = time.monotonic()
        with (output / (label + ".log")).open("w") as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=False)
        (output / (label + ".exit")).write_text(str(result.returncode) + "\n")
        record = {"label": label, "command": command, "cwd": str(root), "within_shared_wrapper_batch": True,
                  "exit": result.returncode, "seconds": time.monotonic() - begin}
        with (output / "commands.jsonl").open("a") as log:
            log.write(json.dumps(record) + "\n")
        print(json.dumps(record), flush=True)
        if result.returncode:
            raise RuntimeError(f"{label}: exit {result.returncode}; no unchanged retry")
        return [json.loads(line) for line in (output / (label + ".log")).read_text().splitlines()
                if line.startswith("{")]

    for kind, order in cases:
        for backend in ("std", "pool"):
            rows = invoke(f"observe-{kind}-{order}-{backend}", "--observe", kind, backend, order)
            results[f"{kind}/{order}/{backend}"] = rows
    (output / "observations.json").write_text(json.dumps(results, indent=2) + "\n")

    signatures = {}
    for sample in range(7):
        for kind, order in cases:
            # Balance backend order across processes. Each backend has a fresh process.
            for backend in (("std", "pool") if sample % 2 == 0 else ("pool", "std")):
                rows = invoke(f"sample-{sample}-{kind}-{order}-{backend}", "--sample", kind, backend, order)
                temperature, phases, total, signature = None, [], 0, []

                def finish():
                    if temperature is None:
                        return
                    assert len(phases) == (22 if kind == "map" else 15), phases
                    samples.append({"container": kind, "order": order, "backend": backend,
                                    "sample": sample, "temperature": temperature,
                                    "phase": "sum", "ns": total})
                    key = (kind, order, temperature)
                    if key in signatures:
                        assert signatures[key] == signature, "matched useful results differ"
                    else:
                        signatures[key] = signature.copy()

                for row in rows:
                    if row["kind"] == "sample":
                        finish()
                        temperature, phases, total, signature = row["temperature"], [], 0, []
                    elif row["kind"] == "timing":
                        phases.append(row["phase"])
                        signature.append((row["phase"], row["checksum"]))
                        total += row["ns"]
                        samples.append({"container": kind, "order": order, "backend": backend,
                                        "sample": sample, "temperature": temperature,
                                        "phase": row["phase"], "ns": row["ns"], "checksum": row["checksum"]})
                finish()
                assert [r["temperature"] for r in rows if r["kind"] == "sample"] == ["cold", "warm"]
    with (output / "samples.jsonl").open("w") as log:
        for row in samples:
            log.write(json.dumps(row) + "\n")
    grouped = {}
    for row in samples:
        key = "/".join(row[k] for k in ("container", "order", "backend", "temperature", "phase"))
        grouped.setdefault(key, []).append(row["ns"])
    summary = {}
    for key, values in grouped.items():
        values.sort()
        assert len(values) == 7
        summary[key] = {"count": len(values), "p50_ns": values[math.ceil(.5 * len(values)) - 1],
                        "p95_ns": values[math.ceil(.95 * len(values)) - 1], "max_ns": values[-1]}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({"complete": True, "native_invocations": 64, "timing_traces": 112,
                      "summary_rows": len(summary), "seconds": time.monotonic() - started}), flush=True)


if __name__ == "__main__":
    main()
