"""Check automatic Engine signal reporting in previously built POSIX consumers.

Never configures or builds the project. Executing the child processes requires
the explicit test authorization specified by AGENTS.md.
"""

import argparse
from pathlib import Path
import os
import re
import signal
import subprocess
import tempfile


def disable_core_files():
    import resource

    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def check_build(build, ndebug):
    executable = build.resolve() / "tests-acceptance-engine-signal"
    with tempfile.TemporaryDirectory(prefix="cheryl-signal-acceptance-") as temporary:
        scope = subprocess.run(
            [str(executable), "scope"], cwd=temporary, capture_output=True,
            text=True, timeout=15, check=False, preexec_fn=disable_core_files,
        )
        if scope.returncode != 0 or scope.stdout.strip() != f"ndebug={int(ndebug)}":
            raise RuntimeError(f"{executable}: unexpected NDEBUG scope\n{scope.stdout}\n{scope.stderr}")
        crash = subprocess.run(
            [str(executable), "crash"], cwd=temporary, capture_output=True,
            text=True, timeout=15, check=False, preexec_fn=disable_core_files,
        )
        # Backward normally forwards SIGABRT; its documented-in-source fallback
        # exits with EXIT_FAILURE if immediate signal forwarding did not terminate.
        expected = (-signal.SIGABRT,) if ndebug else (-signal.SIGABRT, 1)
        if crash.returncode not in expected or "raising SIGABRT" not in crash.stderr:
            raise RuntimeError(f"{executable}: fatal signal did not complete as expected\n{crash.stdout}\n{crash.stderr}")
        header = "Stack trace (most recent call last)"
        reported = header in crash.stderr and re.search(r"^#\d+", crash.stderr, re.MULTILINE)
        if ndebug and header in crash.stderr:
            raise RuntimeError(f"{executable}: NDEBUG consumer installed Backward reporting\n{crash.stderr}")
        if not ndebug and not reported:
            raise RuntimeError(f"{executable}: Debug consumer omitted Backward reporting\n{crash.stderr}")
        print(f"PASS {build}: NDEBUG={int(ndebug)}, automatic Backward reporting={not ndebug}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--debug-build", type=Path)
    parser.add_argument("--release-build", type=Path)
    args = parser.parse_args()
    if os.name != "posix":
        parser.error("this isolated fatal-signal check requires POSIX signal/core-limit support")
    if args.debug_build is None and args.release_build is None:
        parser.error("supply --debug-build and/or --release-build for previously built consumers")
    if args.debug_build is not None:
        check_build(args.debug_build, False)
    if args.release_build is not None:
        check_build(args.release_build, True)


if __name__ == "__main__":
    main()
