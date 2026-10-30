#!/usr/bin/env python3
"""Generate the developer manual and C++ reference without configuring Cheryl."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ElementTree

# Set the cache location before importing repository-local modules.
GENERATION_ROOT = Path(__file__).resolve().parent
GENERATED_ROOT = (GENERATION_ROOT / "generated").resolve()
SITE_ROOT = GENERATION_ROOT.parent
REPO_ROOT = SITE_ROOT.parents[1]
if sys.pycache_prefix is None:
    sys.pycache_prefix = str(GENERATED_ROOT / "pycache")
os.environ["PYTHONPYCACHEPREFIX"] = sys.pycache_prefix

from bootstrap import atomic_json, file_lock, read_json, validate_doxygen
from source_revision import refresh_html_sources


PUBLIC_INCLUDE_ROOTS = tuple(
    REPO_ROOT / name
    for name in (
        "projects/engine/include",
        "projects/modules/runtime/startup/include",
        "projects/modules/runtime/workers/include",
        "projects/modules/platform/debug-terminal-linux/private/startup/include",
        "projects/modules/platform/native-glfw/include",
        "projects/modules/platform/debug-terminal-linux/include",
        "projects/modules/graphics/opengl/include",
        "projects/modules/ui/tgui/include",
        "projects/modules/ui/rmlui/include",
        "projects/modules/audio/miniaudio/include",
    )
)
# Public exceptions retain their historical include path outside include/cheryl.
# Select that owner explicitly rather than exposing every implementation header.
PUBLIC_SUPPORT_HEADER_PATHS = ("projects/engine/include/internals/exceptions.h",)
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
    for relative in PUBLIC_SUPPORT_HEADER_PATHS:
        path = REPO_ROOT / relative
        if not path.is_file():
            raise RuntimeError(f"Missing public API support header: {path}")
        headers.append(path)
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
        "source_dependencies": source_dependencies(commit),
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


def source_dependencies(commit: str) -> dict[str, str]:
    entries = subprocess.check_output(["git", "ls-tree", "-rz", commit], cwd=REPO_ROOT)
    dependencies = {}
    for entry in entries.split(b"\0"):
        if not entry:
            continue
        metadata, filename = entry.split(b"\t", 1)
        mode, _, identity = metadata.split(b" ", 2)
        if mode == b"160000" or filename == b".gitmodules":
            dependencies[filename.decode("utf-8")] = identity.decode("ascii")
    return dependencies


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


@contextmanager
def timed_stage(name: str):
    started = time.monotonic()
    try:
        yield
    finally:
        print(f"{name} took {time.monotonic() - started:.1f} seconds.", flush=True)


def extraction_fingerprint(output: Path, doxygen: str) -> str:
    return fingerprint({
        "format": CACHE_FORMAT,
        # PROJECT_NUMBER records extraction provenance; it does not change the API.
        "configuration": doxygen_configuration(output, ""),
        "headers": {
            path.relative_to(REPO_ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in header_inputs()
        },
        "extractor": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "doxygen": str(Path(doxygen).resolve()),
        "doxygen_sha256": hashlib.sha256(Path(doxygen).read_bytes()).hexdigest(),
    })


def rendering_inputs(output: Path, metadata: dict, xml_files: dict[str, str]) -> dict:
    scripts = {
        path.relative_to(GENERATION_ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted([*GENERATION_ROOT.glob("*.py"), *(GENERATION_ROOT / "extensions").glob("*.py")])
    }
    return {
        "format": CACHE_FORMAT,
        "repository": str(REPO_ROOT),
        "output": str(output),
        # Cached source links into submodules depend on their pinned revisions.
        "source_dependencies": metadata.get("source_dependencies", {}),
        "tools": metadata["tools"],
        "python": str(Path(sys.executable).resolve()),
        "requirements": metadata["requirements_sha256"],
        "native_manifest": metadata["native_manifest_sha256"],
        "scripts": scripts,
        # Breathe records XML dependencies and cannot stat a removed compound.
        "xml_files": sorted(xml_files),
        "readthedocs_version": os.environ.get("READTHEDOCS_VERSION_NAME"),
    }


def rendering_fingerprint(output: Path, metadata: dict, xml_files: dict[str, str]) -> str:
    return fingerprint(rendering_inputs(output, metadata, xml_files))


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


def declared_bases(declaration: ElementTree.Element) -> list[str] | None:
    """Read an unambiguous class-head base list for specialization correction."""
    location = declaration.find("location")
    if location is None or not location.get("file") or not location.get("line"):
        return None
    path = (REPO_ROOT / location.get("file")).resolve()
    if not path.is_relative_to(REPO_ROOT) or not path.is_file():
        return None
    first = int(location.get("line")) - 1
    last = int(location.get("bodystart", location.get("line")))
    text = "\n".join(path.read_text(encoding="utf-8").splitlines()[first:last + 1])
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.DOTALL)
    tokens = re.findall(r"::|[A-Za-z_]\w*|[^\s]", text)
    angle = paren = 0
    seen_type = False
    bases = None
    current = []
    for token in tokens:
        if token in {"class", "struct"} and angle == paren == 0:
            seen_type = True
        elif seen_type and angle == paren == 0:
            if token == "{":
                return [] if bases is None else bases + ["".join(current)]
            if token == ":":
                bases = []
                continue
            if token == "," and bases is not None:
                bases.append("".join(current))
                current = []
                continue
        if bases is not None and token not in {"public", "protected", "private", "virtual"}:
            current.append(token)
        if token == "<":
            angle += 1
        elif token == ">":
            angle -= 1
        elif token == "(":
            paren += 1
        elif token == ")":
            paren -= 1
        if angle < 0 or paren < 0:
            return None
    return None


def preserve_declaration_metadata(declaration: ElementTree.Element) -> bool:
    """Correct known XML losses using retained flags and the declaring source."""
    changed = False
    if declaration.get("kind") in {"class", "struct"} and "<" in declaration.findtext("compoundname", ""):
        references = declaration.findall("basecompoundref")
        expected = declared_bases(declaration) if references else None
        normalize = lambda value: re.sub(r"\s+", "", value).removeprefix("::")
        actual = {normalize("".join(base.itertext())) for base in references}
        # Some Doxygen versions copy a primary template's bases into a partial
        # specialization. Prune only when every declared base can be matched
        # exactly; aliases or an unfamiliar class-head must remain untouched.
        if expected is not None and all(normalize(base) in actual for base in expected):
            allowed = {normalize(base) for base in expected}
            for base in references:
                if normalize("".join(base.itertext())) not in allowed:
                    declaration.remove(base)
                    changed = True
    for member in declaration.findall(".//memberdef"):
        if member.get("kind") not in {"variable", "function"} or member.get("constexpr") != "yes":
            continue
        type_node = member.find("type")
        if type_node is not None and not re.search(r"\bconstexpr\b", "".join(type_node.itertext())):
            # Breathe's parser does not consume the constexpr XML attribute.
            type_node.text = "constexpr " + (type_node.text or "")
            changed = True
    return changed


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
            if preserve_declaration_metadata(declaration):
                changed.add(identifier)
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
        help="Output root containing xml/, html/, and doctrees/ (default: docs/site/generation/generated).",
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
    parser.add_argument("--jobs", type=int, default=1, help="Positive number of Sphinx workers (default: 1).")
    arguments = parser.parse_args()
    if arguments.jobs < 1:
        parser.error("--jobs must be positive.")
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
        extract_key = extraction_fingerprint(output, doxygen)
        xml_files = file_hashes(output / "xml")
        reuse_xml = (
            not arguments.clean
            and state.get("extraction") == extract_key
            and bool(xml_files)
            and state.get("xml") == xml_files
            and (output / "xml/index.xml").is_file()
            and not (output / ".xml-previous").exists()
        )
        if reuse_xml:
            print("Reusing cached Doxygen XML.", flush=True)
        else:
            reasons = []
            if arguments.clean:
                reasons.append("--clean requested")
            if not state.get("extraction"):
                reasons.append("no completed Doxygen stage")
            elif state["extraction"] != extract_key:
                reasons.append("header or extractor inputs changed")
            if not xml_files or not (output / "xml/index.xml").is_file():
                reasons.append("cached XML is missing")
            elif state.get("xml") != xml_files:
                reasons.append("cached XML content changed")
            if (output / ".xml-previous").exists():
                reasons.append("interrupted XML extraction")
            print("Extracting public headers with Doxygen: " + "; ".join(reasons) + ".", flush=True)
            with timed_stage("Doxygen extraction"):
                refresh_xml(output, doxygen, commit, incremental=not arguments.clean)
            xml_files = file_hashes(output / "xml")
        next_state = {"format": CACHE_FORMAT, "extraction": extract_key, "xml": xml_files}
        if arguments.extract_only and reuse_xml:
            return 0
        # XML remains reusable when a later Sphinx or validation stage fails.
        # Omit rendering until Sphinx succeeds: warning failures can save doctrees.
        atomic_json(state_path, next_state)
        if arguments.extract_only:
            return 0

        metadata = provenance(commit, doxygen_version, output)
        render_inputs = rendering_inputs(output, metadata, xml_files)
        render_key = fingerprint(render_inputs)
        presentation_key = presentation_fingerprint(metadata)
        reasons = []
        if arguments.clean:
            reasons.append("--clean requested")
        if not state.get("rendering"):
            reasons.append("no completed Sphinx stage")
        elif state["rendering"] != render_key:
            previous = state.get("rendering_inputs")
            changed = []
            if isinstance(previous, dict):
                changed = [name for name in render_inputs if previous.get(name) != render_inputs[name]]
            labels = ", ".join(changed or ["configuration/tools/XML inventory"])
            reasons.append(f"rendering inputs changed ({labels})")
        if not (output / "doctrees/environment.pickle").is_file():
            reasons.append("cached Sphinx environment is missing")
        if not (output / "html/index.html").is_file():
            reasons.append("cached HTML entry point is missing")
        fresh = bool(reasons)
        if fresh:
            print("Generating fresh Sphinx output: " + "; ".join(reasons) + ".", flush=True)
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
        with timed_stage("Sphinx rendering"):
            subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "sphinx",
                    "-c",
                    str(GENERATION_ROOT),
                    "-b",
                    "html",
                    "-j",
                    str(arguments.jobs),
                    "-W",
                    "--keep-going",
                    "-n",
                    *write_all,
                    "-w",
                    str(output / "sphinx-warnings.log"),
                    "-d",
                    str(output / "doctrees"),
                    str(SITE_ROOT),
                    str(output / "html"),
                ],
                cwd=REPO_ROOT,
                env=environment,
                check=True,
            )
        # Sphinx writes changed pages using the current revision. Refresh links
        # and the revision footer in retained HTML without resolving every API
        # page and rendering the site's navigation again.
        with timed_stage("Source revision refresh"):
            refreshed = refresh_html_sources(output / "html", commit)
            print(f"Updated source revision links in {refreshed} cached HTML pages.", flush=True)
        (output / "html/revision.json").write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        next_state.update(rendering=render_key, rendering_inputs=render_inputs, presentation=presentation_key)
        atomic_json(state_path, next_state)
        # Validation always runs, including retries against completed Sphinx output.
        with timed_stage("Documentation checks"):
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
