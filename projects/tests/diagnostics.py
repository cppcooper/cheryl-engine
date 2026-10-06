#!/usr/bin/env python3
"""Inspect quiet sessions and subsystem file routing from previously built tests."""

import argparse
import re
import subprocess
import tempfile
from pathlib import Path


def read_records(root):
    records = {}
    for name in ("engine", "os-platform", "rendering", "assets", "memory"):
        files = [root / "logs" / f"{name}.log", *sorted((root / "logs").glob(f"{name}.*.log"))]
        records[name] = "".join(path.read_text() for path in files if path.exists())
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--info-stripped", action="store_true")
    args = parser.parse_args()
    executable = args.build_dir.resolve() / "tests-all"
    cases = "runtime_adapter.sequential_frame:runtime_adapter.concurrent_frame:runtime_adapter.routed_event_order"
    with tempfile.TemporaryDirectory(prefix="cheryl-diagnostics-") as temporary:
        root = Path(temporary)
        result = subprocess.run(
            [str(executable), f"--gtest_filter={cases}"], cwd=root, capture_output=True, text=True, timeout=30, check=False
        )
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        records = read_records(root)
        info = {name: [record for record in content.splitlines() if "[info]" in record] for name, content in records.items()}
        if args.info_stripped:
            assert not any(info.values()), records
        else:
            starts = re.findall(r"subsystem=runtime domain=(\d+) context=\d+ operation=session_begin", records["engine"])
            ends = re.findall(r"subsystem=runtime domain=(\d+) operation=session_end outcome=completed", records["engine"])
            capabilities = re.findall(r"subsystem=input domain=(\d+) operation=capabilities", records["os-platform"])
            assert len(starts) == 4 and len(set(starts)) == 4, records
            assert sorted(starts) == sorted(ends), records
            assert sorted(starts) == sorted(capabilities), records
            assert len(info["engine"]) == 8, records  # begin/end per session
            assert len(info["os-platform"]) == 4, records  # input capability per session
        assert "subsystem=input " not in records["engine"], records["engine"]
        assert "subsystem=runtime " not in records["os-platform"], records["os-platform"]
        for name, content in records.items():
            assert "[warning]" not in content and "[error]" not in content, content
            for record in content.splitlines():
                assert f"[{name}]" in record, record
            for payload in ("codepoint=", "scancode=", "payload=", "é"):
                assert payload not in content, content
        assert not (root / "logs/cheryl.log").exists(), "quiet engine initialized legacy log"
        print("PASS quiet sessions, separate files, domain correlation, bounded INFO, and input privacy")

        resources = root / "resources"
        resources.mkdir()
        cases = "asset_preparation.prepared_pixel_ownership:opengl_program_builder.reflection_output:memory.duplicate_returns"
        result = subprocess.run(
            [str(executable), f"--gtest_filter={cases}"], cwd=resources, capture_output=True, text=True, timeout=30, check=False
        )
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        records = read_records(resources)
        for marker in ("prepare_begin", "prepare_end", "upload_begin", "upload_end"):
            assert (f"operation={marker}" in records["assets"]) != args.info_stripped, records["assets"]
        for name, content in records.items():
            for record in content.splitlines():
                assert f"[{name}]" in record, record
            if name != "assets":
                assert "subsystem=assets " not in content, content
            if name != "rendering":
                assert "subsystem=shader " not in content, content
            if name != "memory":
                assert "Cannot return Block" not in content, content
        assert not (resources / "logs/cheryl.log").exists(), "memory return initialized legacy log"
        print("PASS real asset, shader reflection, and memory ownership file routing")


if __name__ == "__main__":
    main()
