#!/usr/bin/env python3
"""Inspect quiet-session records from a previously built test executable."""

import argparse
import re
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--info-stripped", action="store_true")
    args = parser.parse_args()
    executable = args.build_dir.resolve() / "all-tests"
    cases = "runtime_adapter.sequential_frame:runtime_adapter.concurrent_frame:runtime_adapter.routed_event_order"
    with tempfile.TemporaryDirectory(prefix="cheryl-diagnostics-") as temporary:
        root = Path(temporary)
        result = subprocess.run(
            [str(executable), f"--gtest_filter={cases}"], cwd=root, capture_output=True, text=True, timeout=30, check=False
        )
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        log_path = root / "logs/engine.log"
        records = log_path.read_text() if log_path.exists() else ""
        info = [record for record in records.splitlines() if "[info]" in record]
        if args.info_stripped:
            assert not info, records
        else:
            starts = re.findall(r"subsystem=runtime domain=(\d+) context=\d+ operation=session_begin", records)
            ends = re.findall(r"subsystem=runtime domain=(\d+) operation=session_end outcome=completed", records)
            assert len(starts) == 4 and len(set(starts)) == 4, records
            assert sorted(starts) == sorted(ends), records
            assert len(info) == 12, records  # begin, capabilities, end per session
        assert "[warning]" not in records and "[error]" not in records, records
        for payload in ("codepoint=", "scancode=", "payload=", "é"):
            assert payload not in records, records
        print("PASS quiet sessions, domain correlation, bounded INFO, and input privacy")


if __name__ == "__main__":
    main()
