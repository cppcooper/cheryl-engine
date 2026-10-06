"""Run previously built logging acceptance in isolated, timeout-bounded processes.

This script never configures or builds the project. Executing it requires the
explicit test authorization specified by AGENTS.md.
"""

import argparse
from pathlib import Path
import re
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


def check_named_routing(output, result):
    policy = re.search(r"named-routing compiled-warn=([01]) compiled-error=([01]) compiled-debug=([01])", result.stdout)
    assert policy, result.stdout
    warn, error, debug = (value == "1" for value in policy.groups())
    names = ("engine", "os-platform", "rendering", "assets", "memory")
    records = {name: (output / "logs" / f"{name}.log").read_text() for name in names}
    assert {path.name for path in (output / "logs").iterdir()} == {f"{name}.log" for name in names}
    phases = {
        "engine": ("game-default", "native-default", "reopened-category"),
        "os-platform": ("game-default", "native-default"),
        "rendering": ("native-default", "independent-filter"),
        "assets": ("game-default", "native-default"),
        "memory": ("native-default",),
    }
    for name, content in records.items():
        for phase in phases[name]:
            for form in ("formatted", "stream", "direct"):
                marker = f"named-{form} category={name} phase={phase}"
                assert (marker in content) == warn, f"{name}: {marker}\n{content}"
        for record in content.splitlines():
            assert f"[{name}]" in record, record
            assert "closed-category" not in record, record
            assert "filtered-asset" not in record, record
            assert "isolated-secret-event" not in record and "isolated-secret-payload" not in record, record
            for other in names:
                if other != name:
                    assert f"category={other} " not in record, record
        assert "host-game-marker" not in content and "host-native-marker" not in content, content
    assert ("admitted-asset-error" in records["assets"]) == error, records["assets"]
    summaries = [record for record in records["engine"].splitlines() if "subsystem=events " in record]
    assert len(summaries) == (2 if debug else 0), records["engine"]
    for summary in summaries:
        assert "registrations=1 active=1 dispatches=1 invocations=1" in summary, summary
    for name, marker in (("app-game", "host-game-marker"), ("app-native", "host-native-marker")):
        content = (output / f"{name}.log").read_text()
        assert marker in content and f"[{name}]" in content, content
        assert "named-" not in content and "subsystem=" not in content, content
        assert "closed-category" not in content and "admitted-asset-error" not in content, content


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dirs", nargs="+", type=Path)
    args = parser.parse_args()
    for supplied in args.build_dirs:
        build = supplied.resolve()
        executable = build / "tests-acceptance-engine-logging"
        aggregate = build / "tests-engine-logging"
        with tempfile.TemporaryDirectory(prefix="cheryl-logging-acceptance-") as temporary:
            root = Path(temporary)
            run([str(aggregate), "--gtest_filter=logging.*"], root)
            for scenario in ("faults", "rotation", "full-reentry", "discard", "retained", "named-routing", "static", "fatal-destruction"):
                output = root / scenario
                output.mkdir()
                result = run([str(executable), scenario, str(output)], root, 86 if scenario == "fatal-destruction" else 0)
                if scenario == "static":
                    assert "static-final-record" in (output / "isolated-static.log").read_text()
                if scenario == "named-routing":
                    check_named_routing(output, result)
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
