#!/usr/bin/env python3
"""Generate the developer manual and C++ reference without configuring Cheryl."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from bootstrap import file_lock, validate_doxygen


REPO_ROOT = Path(__file__).resolve().parents[2]
HEADER_ROOT = REPO_ROOT / "projects/engine/include"
PILOT_HEADERS = (
    "cheryl/core/game-framework/abstract-game.h",
    "cheryl/core/game-framework/tick-context.h",
    "cheryl/core/game-framework/simulation-scheduler.h",
    "cheryl/core/resources/asset-management/asset-loader.h",
    "cheryl/core/resources/asset-management/sprite-mgr.h",
    "cheryl/text/font-collection.h",
    "cheryl/assets/types/2d/sprite.h",
    "cheryl/assets/types/2d/base/asset2d.h",
    "cheryl/assets/definitions/sprite.h",
    "cheryl/assets/definitions/animation.h",
    "cheryl/assets/definitions/grid.h",
    "cheryl/math/anchor.h",
    "cheryl/templates/asset-mgr.h",
    "cheryl/templates/singleton.h",
)


def quoted(path: Path) -> str:
    return '"' + path.as_posix().replace('"', '\\"') + '"'


def revision() -> str:
    value = os.environ.get("CHERYL_DOCS_REVISION") or os.environ.get(
        "READTHEDOCS_GIT_COMMIT_HASH"
    )
    if not value:
        value = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True
        ).strip()
    if not re.fullmatch(r"[0-9a-fA-F]{40}|[0-9a-fA-F]{64}", value):
        raise RuntimeError("Documentation source links require a full Git commit ID.")
    return value.lower()


def extract(output: Path, doxygen: str, commit: str) -> None:
    headers = [HEADER_ROOT / name for name in PILOT_HEADERS]
    for header in headers:
        if not header.is_file():
            raise RuntimeError(f"Missing API extraction input: {header}")
    template = Path(__file__).with_name("Doxyfile.in").read_text(encoding="utf-8")
    replacements = {
        "REVISION": commit,
        "OUTPUT": quoted(output),
        "REPO_ROOT": quoted(REPO_ROOT),
        "HEADER_ROOT": quoted(HEADER_ROOT),
        "INPUT": " ".join(quoted(header) for header in headers),
    }
    for key, value in replacements.items():
        template = template.replace(f"@{key}@", value)
    output.mkdir(parents=True, exist_ok=True)
    configuration = output / "Doxyfile"
    configuration.write_text(template, encoding="utf-8")
    if (output / "xml").exists():
        shutil.rmtree(output / "xml")
    subprocess.run([doxygen, str(configuration)], cwd=REPO_ROOT, check=True)
    if not (output / "xml/index.xml").is_file():
        raise RuntimeError("Doxygen succeeded without creating xml/index.xml.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=REPO_ROOT / "build/docs",
        help="Output root containing xml/, html/, and doctrees/ (default: build/docs).",
    )
    parser.add_argument(
        "--extract-only", action="store_true", help="Generate Doxygen XML only."
    )
    parser.add_argument("--doxygen", help="Use this Doxygen executable explicitly.")
    arguments = parser.parse_args()
    if os.environ.get("CHERYL_DOCS_TOOLS_ACTIVE") != "1":
        return subprocess.call(
            [
                sys.executable,
                str(Path(__file__).with_name("bootstrap.py")),
                "exec",
                "--",
                "python",
                str(Path(__file__).resolve()),
                *sys.argv[1:],
            ]
        )
    output = arguments.output.expanduser().resolve()
    if output.is_relative_to(REPO_ROOT / "docs"):
        parser.error("The output root must be outside the Sphinx source directory.")
    doxygen = arguments.doxygen or os.environ.get("CHERYL_DOCS_DOXYGEN") or shutil.which(
        "doxygen"
    )
    if not doxygen:
        parser.error("Doxygen is unavailable; prepare the documentation tools first.")
    if arguments.doxygen:
        candidate = Path(arguments.doxygen).expanduser()
        doxygen = (
            str(candidate.resolve())
            if candidate.is_file()
            else shutil.which(arguments.doxygen)
        )
        if not doxygen:
            parser.error(f"Doxygen executable not found: {arguments.doxygen}")
    validate_doxygen(Path(doxygen))
    commit = revision()
    # Every output/profile uses this same lock because Exhale writes source pages.
    with file_lock(REPO_ROOT / "build/.docs-generation.lock"):
        extract(output, doxygen, commit)
        if arguments.extract_only:
            return 0
        generated = REPO_ROOT / "docs/api/generated"
        if generated.exists():
            shutil.rmtree(generated)
        for directory in (output / "html", output / "doctrees"):
            if directory.exists():
                shutil.rmtree(directory)
        environment = os.environ.copy()
        environment["CHERYL_DOCS_XML_DIR"] = str(output / "xml")
        environment["CHERYL_DOCS_REVISION"] = commit
        subprocess.run(
            [
                sys.executable,
                "-m",
                "sphinx",
                "-b",
                "html",
                "-W",
                "--keep-going",
                "-n",
                "-w",
                str(output / "sphinx-warnings.log"),
                "-d",
                str(output / "doctrees"),
                str(REPO_ROOT / "docs"),
                str(output / "html"),
            ],
            cwd=REPO_ROOT,
            env=environment,
            check=True,
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Documentation generation failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
