"""Refresh source identity without rendering unchanged documentation again."""

from __future__ import annotations

from pathlib import Path
import re


SOURCE_REPOSITORY = "https://github.com/cppcooper/cheryl-engine"
SOURCE_PATH = re.compile(r"(?:blob|tree)/(?P<revision>[0-9a-fA-F]{64}|[0-9a-fA-F]{40})(?=[/?#]|$)")
SOURCE_ANCHOR = re.compile(
    r"(?P<before><a\b[^>]*?\shref=)(?P<quote>[\"'])"
    r"(?P<url>https://github\.com/[^\"']+)(?P=quote)"
    r"(?P<after>[^>]*>)(?P<label>[^<]*)"
)


def source_url_at_revision(url: str, revision: str, repository: str = SOURCE_REPOSITORY) -> str:
    # The documentation checker requires same-repository source links to use
    # the build revision; external repositories retain their own pinned commits.
    prefix = repository.rstrip("/") + "/"
    if not url.startswith(prefix):
        return url
    match = SOURCE_PATH.match(url, len(prefix))
    if match is None:
        return url
    start, end = match.span("revision")
    return url[:start] + revision + url[end:]


def refresh_html_sources(directory: Path, revision: str) -> int:
    def refresh(match):
        url = match["url"]
        updated = source_url_at_revision(url, revision)
        if updated == url:
            return match[0]
        label = match["label"]
        if url.startswith(SOURCE_REPOSITORY + "/tree/"):
            previous = url.removeprefix(SOURCE_REPOSITORY + "/tree/")
            if re.fullmatch(r"[0-9a-fA-F]{40}|[0-9a-fA-F]{64}", previous) and label == previous[:12]:
                label = revision[:12]
        quote = match["quote"]
        return match["before"] + quote + updated + quote + match["after"] + label

    changed = 0
    for path in sorted(directory.rglob("*.html")):
        original = path.read_text(encoding="utf-8")
        updated = SOURCE_ANCHOR.sub(refresh, original)
        if updated != original:
            path.write_text(updated, encoding="utf-8")
            changed += 1
    return changed
