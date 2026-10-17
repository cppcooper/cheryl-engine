"""Preserve unchanged API sources and retire removed HTML during local rebuilds."""

from __future__ import annotations

from dataclasses import dataclass, field
import os
from pathlib import Path
import shutil

from sphinx.errors import ExtensionError
from sphinx.util import logging


LOGGER = logging.getLogger(__name__)


@dataclass
class Generation:
    current: Path
    previous: Path
    removed: set[str] = field(default_factory=set)


def restore(generation: Generation) -> None:
    if generation.previous.is_dir():
        if generation.current.exists():
            shutil.rmtree(generation.current)
        generation.previous.replace(generation.current)


def begin(app) -> None:
    current = Path(app.srcdir) / "generation/generated/api"
    generation = Generation(current, current.with_name(".api-previous"))
    app._cheryl_api_generation = generation
    if os.environ.get("CHERYL_DOCS_INCREMENTAL") != "1":
        if generation.previous.exists():
            shutil.rmtree(generation.previous)
        return

    # A stopped process may not have reached build-finished. Recover its complete
    # sources before staging again; the wrapper serializes generation with a lock.
    restore(generation)
    if current.exists():
        current.replace(generation.previous)


def preserve(app) -> None:
    generation = app._cheryl_api_generation
    if not generation.previous.is_dir():
        return

    unchanged = 0
    for current in generation.current.rglob("*"):
        if not current.is_file():
            continue
        previous = generation.previous / current.relative_to(generation.current)
        if not previous.is_file():
            continue
        metadata = previous.stat()
        if current.stat().st_size == metadata.st_size and current.read_bytes() == previous.read_bytes():
            os.utime(current, ns=(metadata.st_atime_ns, metadata.st_mtime_ns))
            unchanged += 1
    LOGGER.info("API sources: preserved timestamps for %d unchanged files", unchanged)


def remember_removed(app, environment, added, changed, removed):
    app._cheryl_api_generation.removed.update(removed)
    return []


def remove_output(path: Path, output: Path) -> None:
    if not path.resolve().is_relative_to(output):
        raise ExtensionError(f"Removed documentation output escapes the HTML directory: {path}")
    path.unlink(missing_ok=True)


def finish(app, exception) -> None:
    generation = getattr(app, "_cheryl_api_generation", None)
    if generation is None:
        return
    if exception is not None:
        restore(generation)
        return

    if app.builder.name == "html":
        output = Path(app.outdir).resolve()
        for document in generation.removed:
            remove_output(Path(app.builder.get_outfilename(document)), output)
            for suffix in app.config.source_suffix:
                source = document + suffix
                if suffix != app.config.html_sourcelink_suffix:
                    source += app.config.html_sourcelink_suffix
                remove_output(output / "_sources" / source, output)

    if generation.previous.exists():
        shutil.rmtree(generation.previous)


def setup(app) -> dict[str, object]:
    # Exhale and the metadata extensions run at the default priority of 500.
    # Compare only after every generator has finished, before Sphinx reads files.
    app.connect("builder-inited", begin, priority=100)
    app.connect("builder-inited", preserve, priority=900)
    app.connect("env-get-outdated", remember_removed)
    app.connect("build-finished", finish)
    return {"version": "1", "parallel_read_safe": True, "parallel_write_safe": True}
