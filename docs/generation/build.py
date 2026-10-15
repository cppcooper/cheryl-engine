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

# Set the cache location before importing repository-local modules.
GENERATION_ROOT = Path(__file__).resolve().parent
GENERATED_ROOT = (GENERATION_ROOT / "generated").resolve()
REPO_ROOT = GENERATION_ROOT.parents[1]
if sys.pycache_prefix is None:
    sys.pycache_prefix = str(GENERATED_ROOT / "pycache")
os.environ["PYTHONPYCACHEPREFIX"] = sys.pycache_prefix

from bootstrap import atomic_json, file_lock, read_json, validate_doxygen


PUBLIC_INCLUDE_ROOTS = tuple(
    REPO_ROOT / name
    for name in (
        "projects/engine/include",
        "projects/engine/support/startup/include",
        "projects/engine/support/worker-topology/include",
        "projects/engine/support/debug-terminal/include",
        "projects/modules/platform/native-glfw/include",
        "projects/modules/platform/debug-terminal-linux/include",
        "projects/modules/graphics/opengl/include",
        "projects/modules/graphics/opengl/support/startup/include",
        "projects/modules/ui/tgui/include",
        "projects/modules/ui/rmlui/include",
        "projects/modules/audio/miniaudio/include",
    )
)
HEADER_SUFFIXES = {".h", ".hpp", ".inl", ".ipp", ".tpp"}
CACHE_FORMAT = 1


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
            Path(__file__).with_name("requirements.txt").read_bytes()
        ).hexdigest(),
        "native_manifest_sha256": hashlib.sha256(
            Path(__file__).with_name("native-tools.json").read_bytes()
        ).hexdigest(),
    }


def doxygen_configuration(output: Path, commit: str) -> str:
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
    return template


def file_hashes(directory: Path, pattern: str = "*") -> dict[str, str]:
    return {
        path.relative_to(directory).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(directory.rglob(pattern))
        if path.is_file()
    }


def fingerprint(value: dict) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def extraction_fingerprint(output: Path, doxygen: str, commit: str) -> str:
    return fingerprint({
        "format": CACHE_FORMAT,
        "configuration": doxygen_configuration(output, commit),
        "headers": {
            path.relative_to(REPO_ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in header_inputs()
        },
        "extractor": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "doxygen": str(Path(doxygen).resolve()),
        "doxygen_sha256": hashlib.sha256(Path(doxygen).read_bytes()).hexdigest(),
    })


def rendering_fingerprint(output: Path, metadata: dict, xml_files: dict[str, str]) -> str:
    scripts = {
        path.relative_to(GENERATION_ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted([*GENERATION_ROOT.glob("*.py"), *(GENERATION_ROOT / "extensions").glob("*.py")])
    }
    return fingerprint({
        "format": CACHE_FORMAT,
        "repository": str(REPO_ROOT),
        "output": str(output),
        "revision": metadata["source_revision"],
        "tools": metadata["tools"],
        "python": str(Path(sys.executable).resolve()),
        "requirements": metadata["requirements_sha256"],
        "native_manifest": metadata["native_manifest_sha256"],
        "scripts": scripts,
        # Breathe records XML dependencies and cannot stat a removed compound.
        "xml_files": sorted(xml_files),
        "readthedocs_version": os.environ.get("READTHEDOCS_VERSION_NAME"),
    })


def presentation_fingerprint(metadata: dict) -> str:
    return fingerprint({
        "templates": file_hashes(GENERATION_ROOT / "templates"),
        "static": file_hashes(GENERATION_ROOT / "static"),
        "dirty": metadata["dirty"],
    })


def refresh_xml(output: Path, doxygen: str, commit: str, *, incremental: bool) -> None:
    xml = output / "xml"
    previous = output / ".xml-previous"
    # Recover an interrupted extraction before staging its replacement.
    if previous.exists():
        if xml.exists():
            shutil.rmtree(xml)
        previous.replace(xml)
    if xml.exists():
        xml.replace(previous)
    try:
        extract(output, doxygen, commit)
        if incremental and previous.exists():
            for path in xml.rglob("*"):
                original = previous / path.relative_to(xml)
                if path.is_file() and original.is_file() and path.read_bytes() == original.read_bytes():
                    status = original.stat()
                    os.utime(path, ns=(status.st_atime_ns, status.st_mtime_ns))
    except BaseException:
        if xml.exists():
            shutil.rmtree(xml)
        if previous.exists():
            previous.replace(xml)
        raise
    if previous.exists():
        shutil.rmtree(previous)


def extract(output: Path, doxygen: str, commit: str) -> None:
    template = doxygen_configuration(output, commit)
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
    configuration_xml = ElementTree.parse(output / "xml/Doxyfile.xml")
    private_namespaces = {
        value.text[:-3]
        for value in configuration_xml.findall("option[@id='EXCLUDE_SYMBOLS']/value")
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
            # Doxygen can merge a leading global return-type qualifier with
            # `virtual`. Breathe already renders virtuality from the XML attribute.
            for member in declaration.findall(".//memberdef[@kind='function']"):
                if member.get("virt") not in {"virtual", "pure-virtual"}:
                    continue
                for tag in ("type", "definition"):
                    value = member.find(tag)
                    if value is not None and value.text and value.text.startswith("virtual::"):
                        value.text = value.text.removeprefix("virtual")
                        changed.add(identifier)
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
        default=GENERATED_ROOT,
        help="Output root containing xml/, html/, and doctrees/ (default: docs/generation/generated).",
    )
    parser.add_argument(
        "--extract-only", action="store_true", help="Generate Doxygen XML only."
    )
    parser.add_argument(
        "--clean", action="store_true", help="Regenerate XML, API pages and HTML without reusing build caches."
    )
    parser.add_argument(
        "--force-pinned", action="store_true", help="Select the locked documentation tools instead of compatible installed tools."
    )
    parser.add_argument("--cache-dir", type=Path, help="Shared documentation tool cache directory.")
    parser.add_argument("--doxygen", help="Use this Doxygen executable explicitly.")
    arguments = parser.parse_args()
    force_pinned = arguments.force_pinned or os.environ.get("CHERYL_DOCS_FORCE_PINNED") == "1"
    if arguments.doxygen is not None and force_pinned:
        parser.error("An explicit Doxygen override cannot be combined with forced pinned tools.")
    select_tools = os.environ.get("CHERYL_DOCS_TOOLS_ACTIVE") != "1"
    if arguments.force_pinned and os.environ.get("CHERYL_DOCS_FORCE_PINNED") != "1":
        select_tools = True
    if arguments.cache_dir is not None:
        selected_cache = os.environ.get("CHERYL_DOCS_CACHE_DIR")
        if selected_cache is None or arguments.cache_dir.expanduser().resolve() != Path(selected_cache).expanduser().resolve():
            select_tools = True
    if select_tools:
        overrides = ["--doxygen", arguments.doxygen] if arguments.doxygen is not None else []
        if force_pinned:
            overrides.append("--force-pinned")
        if arguments.cache_dir is not None:
            overrides.extend(["--cache-dir", str(arguments.cache_dir)])
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
    if ((output.is_relative_to(REPO_ROOT / "docs") and not output.is_relative_to(GENERATED_ROOT))
            or (REPO_ROOT / "docs").is_relative_to(output)
            or any(root.is_relative_to(output) for root in PUBLIC_INCLUDE_ROOTS)
            or output.is_relative_to(GENERATED_ROOT / "api")
            or output.is_relative_to(GENERATED_ROOT / ".api-previous")
            or output.is_relative_to(GENERATED_ROOT / ".xml-previous")
            or output.is_relative_to(GENERATED_ROOT / "pycache")):
        parser.error("The output root must not overwrite authored sources, generated API pages or Python caches.")
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
    # Every output/profile uses this same lock because Exhale writes source pages.
    with file_lock(GENERATED_ROOT / ".generation.lock"):
        commit = revision()
        output.mkdir(parents=True, exist_ok=True)
        state_path = output / "build-state.json"
        state = read_json(state_path)
        if not isinstance(state, dict) or state.get("format") != CACHE_FORMAT:
            state = {}
        extract_key = extraction_fingerprint(output, doxygen, commit)
        xml_files = file_hashes(output / "xml")
        reuse_xml = (
            not arguments.clean
            and state.get("extraction") == extract_key
            and bool(xml_files)
            and state.get("xml") == xml_files
            and (output / "xml/index.xml").is_file()
            and not (output / ".xml-previous").exists()
        )
        # Sphinx can save doctrees before failing on warnings. Never reuse an
        # interrupted/failed run as a successful baseline on the next invocation.
        state_path.unlink(missing_ok=True)
        if reuse_xml:
            print("Reusing cached Doxygen XML.", flush=True)
        else:
            print("Extracting public headers with Doxygen.", flush=True)
            refresh_xml(output, doxygen, commit, incremental=not arguments.clean)
            xml_files = file_hashes(output / "xml")
        next_state = {"format": CACHE_FORMAT, "extraction": extract_key, "xml": xml_files}
        if arguments.extract_only:
            atomic_json(state_path, next_state)
            return 0

        metadata = provenance(commit, doxygen_version, output)
        render_key = rendering_fingerprint(output, metadata, xml_files)
        presentation_key = presentation_fingerprint(metadata)
        fresh = (
            arguments.clean
            or state.get("rendering") != render_key
            or not (output / "doctrees/environment.pickle").is_file()
            or not (output / "html/index.html").is_file()
        )
        if fresh:
            print("Generating fresh Sphinx output.", flush=True)
            for directory in (GENERATED_ROOT / "api", GENERATED_ROOT / ".api-previous",
                              output / "html", output / "doctrees"):
                if directory.exists():
                    shutil.rmtree(directory)
        else:
            print("Updating Sphinx output incrementally.", flush=True)

        environment = os.environ.copy()
        environment["CHERYL_DOCS_XML_DIR"] = str(output / "xml")
        environment["CHERYL_DOCS_REVISION"] = commit
        environment["CHERYL_DOCS_INCREMENTAL"] = "0" if arguments.clean else "1"
        environment["CHERYL_DOCS_DIRTY"] = "1" if metadata["dirty"] else "0"
        # CSS/template changes and the dirty-source banner affect every HTML
        # page, but do not require reparsing unchanged source documents.
        write_all = ["-a"] if not fresh and state.get("presentation") != presentation_key else []
        subprocess.run(
            [
                sys.executable,
                "-m",
                "sphinx",
                "-c",
                str(GENERATION_ROOT),
                "-b",
                "html",
                "-W",
                "--keep-going",
                "-n",
                *write_all,
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
        next_state.update(rendering=render_key, presentation=presentation_key)
        atomic_json(state_path, next_state)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Documentation generation failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
