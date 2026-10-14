#!/usr/bin/env python3
"""Generate the developer manual and C++ reference without configuring Cheryl."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ElementTree

from bootstrap import file_lock, validate_doxygen


REPO_ROOT = Path(__file__).resolve().parents[2]
PUBLIC_INCLUDE_ROOTS = tuple(
    REPO_ROOT / name
    for name in (
        "projects/engine/include",
        "projects/engine/support/startup/include",
        "projects/engine/support/worker-topology/include",
        "projects/engine/support/debug-terminal/include",
        "projects/modules/platform/native-glfw/include",
        "projects/modules/graphics/opengl/include",
        "projects/modules/graphics/opengl/support/startup/include",
        "projects/modules/ui/tgui/include",
        "projects/modules/ui/rmlui/include",
        "projects/modules/audio/miniaudio/include",
    )
)
HEADER_SUFFIXES = {".h", ".hpp", ".inl", ".ipp", ".tpp"}


def header_inputs() -> list[Path]:
    headers = []
    for include_root in PUBLIC_INCLUDE_ROOTS:
        source = include_root / "cheryl"
        if not source.is_dir():
            raise RuntimeError(f"Missing public API include root: {source}")
        headers.extend(
            path
            for path in source.rglob("*")
            if path.is_file() and path.suffix in HEADER_SUFFIXES
        )
    return sorted(headers, key=lambda path: path.as_posix())


def quoted(path: Path) -> str:
    return '"' + path.as_posix().replace('"', '\\"') + '"'


def revision() -> str:
    head = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True
    ).strip().lower()
    value = os.environ.get("CHERYL_DOCS_REVISION") or os.environ.get(
        "READTHEDOCS_GIT_COMMIT_HASH"
    )
    if not value:
        value = head
    if not re.fullmatch(r"[0-9a-fA-F]{40}|[0-9a-fA-F]{64}", value):
        raise RuntimeError("Documentation source links require a full Git commit ID.")
    if value.lower() != head:
        raise RuntimeError("The documentation revision must match the checked-out Git HEAD.")
    return head


def provenance(commit: str, doxygen_version: str, output: Path) -> dict:
    tools = json.loads(os.environ.get("CHERYL_DOCS_TOOLCHAIN", "{}"))
    if (not isinstance(tools, dict) or tools.get("python_source") not in {"system", "pinned"}
            or not isinstance(tools.get("packages"), dict)):
        raise RuntimeError("Documentation tool identity is missing; invoke the generator through bootstrap.")
    ref = (
        os.environ.get("CHERYL_DOCS_REF")
        or os.environ.get("READTHEDOCS_GIT_IDENTIFIER")
        or os.environ.get("GITHUB_REF")
    )
    if not ref:
        result = subprocess.run(
            ["git", "symbolic-ref", "--quiet", "--short", "HEAD"],
            cwd=REPO_ROOT, capture_output=True, text=True,
        )
        ref = result.stdout.strip() if result.returncode == 0 else "detached"
    paths = ["."]
    if output.is_relative_to(REPO_ROOT):
        paths.append(":(exclude)" + output.relative_to(REPO_ROOT).as_posix())
    dirty = bool(subprocess.check_output(
        ["git", "status", "--porcelain", "--untracked-files=normal", "--", *paths],
        cwd=REPO_ROOT, text=True,
    ).strip())
    return {
        "schema_version": 1,
        "source_repository": "https://github.com/cppcooper/cheryl-engine",
        "source_revision": commit,
        "source_ref": ref,
        "dirty": dirty,
        "header_count": len(header_inputs()),
        "tools": {
            "python": platform.python_version(),
            "python_source": tools["python_source"],
            "doxygen": doxygen_version,
            "packages": tools["packages"],
        },
        "requirements_sha256": hashlib.sha256(
            (REPO_ROOT / "docs/requirements.txt").read_bytes()
        ).hexdigest(),
        "native_manifest_sha256": hashlib.sha256(
            Path(__file__).with_name("native-tools.json").read_bytes()
        ).hexdigest(),
    }


def extract(output: Path, doxygen: str, commit: str) -> None:
    headers = header_inputs()
    template = Path(__file__).with_name("Doxyfile.in").read_text(encoding="utf-8")
    replacements = {
        "REVISION": commit,
        "OUTPUT": quoted(output),
        "REPO_ROOT": quoted(REPO_ROOT),
        "INCLUDE_ROOTS": " ".join(quoted(root) for root in PUBLIC_INCLUDE_ROOTS),
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
    # Doxygen's XML includes private nested compounds even with EXTRACT_PRIVATE=NO.
    # Keep their declarations available for exact source links, but remove them
    # from the public index and relationship navigation before Exhale reads it.
    index_path = output / "xml/index.xml"
    index = ElementTree.parse(index_path)
    doxygen_configuration = ElementTree.parse(output / "xml/Doxyfile.xml")
    private_namespaces = {
        value.text[:-3]
        for value in doxygen_configuration.findall("option[@id='EXCLUDE_SYMBOLS']/value")
        if value.text and value.text.endswith("::*")
    }
    declarations = {}
    private = set()
    changed = set()
    for compound in index.findall("compound"):
        identifier = compound.get("refid")
        document = ElementTree.parse(output / "xml" / (identifier + ".xml"))
        declarations[identifier] = document
        declaration = document.find("compounddef")
        if declaration is not None:
            # Qualified implementation friendships are sometimes reported public.
            # Apply only the explicit namespace policy, preserving public friends.
            for member in declaration.findall(".//memberdef[@kind='friend']"):
                name = member.findtext("name", "")
                scope = declaration.findtext("compoundname", "")
                while True:
                    qualified = "::".join(part for part in (scope, name) if part).removeprefix("::")
                    if any(qualified.startswith(namespace + "::") for namespace in private_namespaces):
                        member.set("prot", "private")
                        for reference in declaration.findall("listofallmembers/member"):
                            if reference.get("refid") == member.get("id"):
                                reference.set("prot", "private")
                        changed.add(identifier)
                        break
                    if not scope:
                        break
                    scope = scope.rsplit("::", 1)[0] if "::" in scope else ""
        if declaration is not None and declaration.get("prot") == "private":
            private.add(identifier)
    pending = list(private)
    while pending:
        declaration = declarations[pending.pop()].find("compounddef")
        for reference in declaration.findall("innerclass"):
            identifier = reference.get("refid")
            if identifier in declarations and identifier not in private:
                private.add(identifier)
                pending.append(identifier)
    if private:
        for compound in list(index.findall("compound")):
            if compound.get("refid") in private:
                index.getroot().remove(compound)
        index.write(index_path, encoding="utf-8", xml_declaration=True)
        for identifier, document in declarations.items():
            declaration = document.find("compounddef")
            if declaration is None:
                continue
            hidden = [reference for reference in declaration.findall("innerclass") if reference.get("refid") in private]
            for reference in hidden:
                declaration.remove(reference)
            if hidden:
                changed.add(identifier)
    for identifier in changed:
        declarations[identifier].write(
            output / "xml" / (identifier + ".xml"), encoding="utf-8", xml_declaration=True
        )


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
    if arguments.doxygen is not None and os.environ.get("CHERYL_DOCS_FORCE_PINNED") == "1":
        parser.error("An explicit Doxygen override cannot be combined with forced pinned tools.")
    if os.environ.get("CHERYL_DOCS_TOOLS_ACTIVE") != "1":
        overrides = ["--doxygen", arguments.doxygen] if arguments.doxygen is not None else []
        return subprocess.call(
            [
                sys.executable,
                str(Path(__file__).with_name("bootstrap.py")),
                "exec",
                *overrides,
                "--",
                "python",
                str(Path(__file__).resolve()),
                *sys.argv[1:],
            ]
        )
    output = arguments.output.expanduser().resolve()
    if (output.is_relative_to(REPO_ROOT / "docs")
            or (REPO_ROOT / "docs").is_relative_to(output)
            or any(root.is_relative_to(output) for root in PUBLIC_INCLUDE_ROOTS)):
        parser.error("The output root must not contain or overwrite documentation/public header sources.")
    doxygen = arguments.doxygen or os.environ.get("CHERYL_DOCS_DOXYGEN") or shutil.which(
        "doxygen"
    )
    if not doxygen:
        parser.error("Doxygen is unavailable; prepare the documentation tools first.")
    if arguments.doxygen is not None:
        candidate = Path(arguments.doxygen).expanduser()
        doxygen = (
            str(candidate.resolve())
            if candidate.is_file()
            else shutil.which(arguments.doxygen)
        )
        if not doxygen:
            parser.error(f"Doxygen executable not found: {arguments.doxygen}")
    doxygen_version = validate_doxygen(Path(doxygen))
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
        metadata = provenance(commit, doxygen_version, output)
        environment["CHERYL_DOCS_DIRTY"] = "1" if metadata["dirty"] else "0"
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
        (output / "html/revision.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        subprocess.run(
            [sys.executable, str(Path(__file__).with_name("check.py")),
             "--html", str(output / "html"), "--xml", str(output / "xml")],
            cwd=REPO_ROOT, check=True,
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Documentation generation failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
