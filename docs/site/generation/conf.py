"""Sphinx configuration shared by local builds and publication."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


GENERATION_ROOT = Path(__file__).resolve().parent
GENERATED_ROOT = (GENERATION_ROOT / "generated").resolve()
SITE_ROOT = GENERATION_ROOT.parent
REPO_ROOT = SITE_ROOT.parents[1]
if sys.pycache_prefix is None:
    sys.pycache_prefix = str(GENERATED_ROOT / "pycache")
os.environ["PYTHONPYCACHEPREFIX"] = sys.pycache_prefix
sys.path.insert(0, str(GENERATION_ROOT))
sys.path.insert(0, str(GENERATION_ROOT / "extensions"))

project = "Cheryl Engine"
author = "Cheryl Engine contributors"
copyright = "2026, Cheryl Engine contributors"
source_revision = os.environ.get("CHERYL_DOCS_REVISION") or subprocess.check_output(
    ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True
).strip()
# Revision identity is refreshed separately from Sphinx's parsed-page cache.
release = os.environ.get("READTHEDOCS_VERSION_NAME", "")
version = release
source_repository = "https://github.com/cppcooper/cheryl-engine"
source_repository_root = str(REPO_ROOT)

extensions = ["myst_parser", "sphinxcontrib.mermaid", "breathe", "exhale", "api_rendering", "repository_links", "inline_api", "incremental_api"]
# Render ordinary Mermaid fences in the browser using bundled, pinned assets.
myst_fence_as_directive = ["mermaid"]
mermaid_output_format = "raw"
mermaid_use_local = "vendor/mermaid-11.12.1/mermaid.esm.min.mjs"
source_suffix = {".md": "markdown", ".rst": "restructuredtext"}
root_doc = "index"
exclude_patterns = [
    "generation/generated/.api-previous/**",
    "**/.DS_Store",
]
# Only generated API pages participate in source discovery inside this area.
exclude_patterns.extend(
    "generation/generated/" + name + "/**"
    for name in ("html", "xml", "doctrees", "pycache")
)
for directory, included in ((GENERATION_ROOT, "generated"), (GENERATED_ROOT, "api")):
    if directory.is_dir():
        exclude_patterns.extend(
            path.relative_to(SITE_ROOT).as_posix() + ("/**" if path.is_dir() else "")
            for path in sorted(directory.iterdir())
            if path.name != included
        )
# Cold and warm output directories must produce the same Sphinx cache settings.
exclude_patterns = sorted(set(exclude_patterns))
myst_heading_anchors = 6
myst_ref_domains = ["std", "cpp"]
nitpicky = True
show_warning_types = True

# Identifiers supplied by third-party libraries are outside Cheryl's reference.
# All Cheryl class/function links and identifiers remain checked.
nitpick_ignore_regex = [
    ("cpp:identifier", r"std(?:::.*)?"),
    ("cpp:identifier", r"(?:glm|CLI|gainput|tgui|Rml|spdlog)(?:::.*)?"),
    ("cpp:identifier", r"(?:size_t|uintptr_t|u?int(?:8|16|32|64)_t)"),
    ("cpp:identifier", r"(?:GLuint|GLint|GLenum|GLFWwindow|GLFWmonitor|GL_CLAMP_TO_EDGE|GL_RED)"),
    # The alias itself is documented; these members belong to std::steady_clock.
    ("cpp:identifier", r"(?:::)?(?:CE::Runtime::Simulation::)?Clock::(?:duration|time_point|now)"),
    ("cpp:identifier", r"(?:::)?(?:CE::Input::)?ObservationClock::(?:duration|time_point|now)"),
    # This constant belongs to the trait's std::true_type/std::false_type base.
    ("cpp:identifier", r"(?:CE::Time::)?is_chrono_duration<.*>::value"),
]

xml_directory = Path(
    os.environ.get("CHERYL_DOCS_XML_DIR", str(GENERATED_ROOT / "xml"))
).resolve()
breathe_projects = {"Cheryl": str(xml_directory)}
breathe_default_project = "Cheryl"
breathe_domain_by_extension = {"h": "cpp", "hpp": "cpp"}
breathe_default_members = ("members", "undoc-members")
breathe_show_include = True
breathe_show_define_initializer = False
exhale_args = {
    "containmentFolder": str(GENERATED_ROOT / "api"),
    "rootFileName": "library_root.rst",
    "rootFileTitle": "C++ API reference",
    "doxygenStripFromPath": str(REPO_ROOT),
    "exhaleExecutesDoxygen": False,
    "createTreeView": False,
}

html_theme = "sphinx_rtd_theme"
templates_path = ["templates"]
html_static_path = ["static"]
html_css_files = ["hierarchy.css", "typography.css", "layout.css", "theme.css", "copy-code.css"]
html_js_files = ["appearance.js", "copy-code.js"]
html_title = "Cheryl Engine documentation"
# Keep inactive branches in the HTML so the theme can show their expand buttons;
# the theme's CSS still hides their children until expanded.
html_theme_options = {"navigation_depth": 4, "collapse_navigation": False}
html_show_sourcelink = True
html_context = {
    "display_github": True,
    "github_user": "cppcooper",
    "github_repo": "cheryl-engine",
    "conf_py_path": "/docs/site/",
    "source_dirty": os.environ.get("CHERYL_DOCS_DIRTY") == "1",
}
