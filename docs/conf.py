"""Sphinx configuration shared by local builds and publication."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


DOCS_ROOT = Path(__file__).resolve().parent
REPO_ROOT = DOCS_ROOT.parent
if sys.pycache_prefix is None:
    sys.pycache_prefix = str(DOCS_ROOT / "generated/pycache")
os.environ["PYTHONPYCACHEPREFIX"] = sys.pycache_prefix
sys.path.insert(0, str(DOCS_ROOT / "_ext"))

project = "Cheryl Engine"
author = "Cheryl Engine contributors"
copyright = "2026, Cheryl Engine contributors"
source_revision = os.environ.get("CHERYL_DOCS_REVISION") or subprocess.check_output(
    ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT, text=True
).strip()
release = os.environ.get("READTHEDOCS_VERSION_NAME", source_revision[:12])
version = release
source_repository = "https://github.com/cppcooper/cheryl-engine"
source_repository_root = str(REPO_ROOT)

extensions = ["myst_parser", "breathe", "exhale", "api_rendering", "repository_links", "inline_api"]
source_suffix = {".md": "markdown", ".rst": "restructuredtext"}
root_doc = "index"
exclude_patterns = [
    "README.md",
    "STATUS.md",
    "planning/**",
    "testing-requests.md",
    "external-work/**",
    "_ext/**",
    "generated/pycache/**",
    # Ignore API pages left behind by older checkouts.
    "api/generated/**",
    "**/.DS_Store",
]
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
    os.environ.get("CHERYL_DOCS_XML_DIR", str(REPO_ROOT / "build/docs/xml"))
).resolve()
breathe_projects = {"Cheryl": str(xml_directory)}
breathe_default_project = "Cheryl"
breathe_domain_by_extension = {"h": "cpp", "hpp": "cpp"}
breathe_default_members = ("members", "undoc-members")
breathe_show_include = True
breathe_show_define_initializer = False
exhale_args = {
    "containmentFolder": str(DOCS_ROOT / "generated/api"),
    "rootFileName": "library_root.rst",
    "rootFileTitle": "C++ API reference",
    "doxygenStripFromPath": str(REPO_ROOT),
    "exhaleExecutesDoxygen": False,
    "createTreeView": False,
}

html_theme = "sphinx_rtd_theme"
templates_path = ["_templates"]
html_static_path = ["_static"]
html_css_files = ["hierarchy.css"]
html_title = "Cheryl Engine documentation"
html_theme_options = {"navigation_depth": 4, "collapse_navigation": False}
html_show_sourcelink = True
html_context = {
    "display_github": True,
    "github_user": "cppcooper",
    "github_repo": "cheryl-engine",
    "github_version": source_revision,
    "conf_py_path": "/docs/",
    "source_revision": source_revision,
    "source_dirty": os.environ.get("CHERYL_DOCS_DIRTY") == "1",
}
