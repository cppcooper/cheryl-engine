"""Run previously built logging acceptance in isolated, timeout-bounded processes.

This script never configures or builds the project. Executing it requires the
explicit test authorization specified by AGENTS.md.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile


def run(command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=30)
    if result.returncode != expected:
        raise RuntimeError(
            f"{command}: expected {expected}, got {result.returncode}\n"
            f"{result.stdout}\n{result.stderr}"
        )
    print(f"PASS {Path(command[0]).name} {' '.join(map(str, command[1:]))}", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dirs", nargs="+", type=Path)
    args = parser.parse_args()
    for supplied in args.build_dirs:
        build = supplied.resolve()
        executable = build / "cheryl-logging-acceptance"
        aggregate = build / "all-tests"
        with tempfile.TemporaryDirectory(prefix="cheryl-logging-acceptance-") as temporary:
            root = Path(temporary)
            run([str(aggregate), "--gtest_filter=logging.*"], root)
            for scenario in ("faults", "rotation", "full-reentry", "discard", "retained", "static", "fatal-destruction"):
                output = root / scenario
                output.mkdir()
                result = run([str(executable), scenario, str(output)], root, 86 if scenario == "fatal-destruction" else 0)
                if scenario == "static":
                    assert "static-final-record" in (output / "isolated-static.log").read_text()
                if scenario == "discard":
                    content = (output / "isolated-target.log").read_text()
                    assert "discarded-marker" not in content
                    assert "recursive-full-queue" not in content
                    markers = [f"accepted[{index}]" for index in range(8192)]
                    previous = -1
                    for marker in markers:
                        position = content.find(marker, previous + 1)
                        assert position > previous, f"missing or reordered {marker}"
                        previous = position
                    assert "discarded=2" in result.stderr
                if scenario == "fatal-destruction":
                    assert "logger destruction from a backend callback" in result.stderr


if __name__ == "__main__":
    main()
