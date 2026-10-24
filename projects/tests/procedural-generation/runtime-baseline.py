#!/usr/bin/env python3
"""Finite context recorder/runner, invoked only through the shared build wrapper.

It runs one real native baseline process with a40s whole-process watchdog. Raw
stdout/stderr and the immediate exit remain separate. No agent/service, allocator
interception, dependency substitution or scheduler is involved.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time


def read(path):
    try:
        return Path(path).read_text()
    except OSError as error:
        return {"unavailable": str(error)}


def context():
    raw = read('/proc/self/cgroup')
    groups = []
    if isinstance(raw, str):
        for line in raw.splitlines():
            fields = line.split(':', 2)
            if len(fields) == 3 and fields[0] == '0':
                current = Path('/sys/fs/cgroup') / fields[2].lstrip('/')
                # Read only this owned resource path and its limits, not processes.
                for _ in range(16):
                    if not current.is_dir():
                        groups.append({'path': str(current), 'unavailable': True})
                        break
                    groups.append({'path': str(current), **{name: read(current/name) for name in
                                  ('memory.current', 'memory.max', 'memory.high', 'memory.swap.max',
                                   'memory.events', 'cpu.stat', 'cpu.max')}})
                    if current == Path('/sys/fs/cgroup'):
                        break
                    current = current.parent
    cpu = read('/proc/cpuinfo')
    model = next((line for line in cpu.splitlines() if line.startswith('model name')), 'unavailable') if isinstance(cpu, str) else cpu
    return {'monotonic_ns': time.monotonic_ns(), 'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
            'platform': platform.platform(), 'cpu_model': model, 'affinity': sorted(os.sched_getaffinity(0)),
            'loadavg': os.getloadavg(), 'meminfo': read('/proc/meminfo'), 'self_cgroup': raw, 'cgroup_limits': groups,
            'exclusions': 'Python/context/hash/printing outside update timers; native thread CPU clock and steady wall clock in raw samples. No allocation/copy instrumentation or CE domain until pinned dependencies are available.'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--logs', type=Path, required=True)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    args.logs.mkdir(parents=True, exist_ok=True)
    before = context()
    before['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
    (args.logs/'context-before.json').write_text(json.dumps(before, indent=2)+'\n')
    started = time.monotonic()
    with (args.logs/'baseline-raw.jsonl').open('w') as stdout, (args.logs/'baseline.stderr.log').open('w') as stderr:
        try:
            result = subprocess.run([str(binary)], stdout=stdout, stderr=stderr, timeout=40, check=False)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
            stderr.write('Finite40s baseline-process watchdog expired; incomplete raw samples retained.\n')
    after = context()
    after['elapsed_runner_seconds'] = time.monotonic()-started
    after['native_exit'] = code
    (args.logs/'context-after.json').write_text(json.dumps(after, indent=2)+'\n')
    (args.logs/'baseline-native-exit.txt').write_text(str(code)+'\n')
    print(json.dumps({'native_exit': code, 'raw': str(args.logs/'baseline-raw.jsonl'), 'runner_seconds': after['elapsed_runner_seconds']}))
    return code


if __name__ == '__main__':
    raise SystemExit(main())
