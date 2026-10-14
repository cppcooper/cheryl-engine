# Generating and publishing documentation

The manual and C++ reference share one generation command. Doxygen extracts the
selected public headers to XML; Breathe and Exhale turn that XML into Sphinx pages,
and MyST includes the Markdown guides. Documentation generation does not configure
or compile the engine.

## Generate locally

Use a Git checkout and Python 3.11 or newer with the standard `venv` module. You
do not need to install Sphinx, Doxygen or pip manually. The first fallback setup
needs network access to PyPI and the official Doxygen release assets, and a writable
user cache. Use `python3` in the commands below if that is your interpreter's name.
Run the shell blocks from any directory inside the checkout; the subshell locates
the repository root and preserves your starting directory.

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  python tools/docs/build.py
)
```

Open `build/docs/html/index.html`. The output directory also contains Doxygen XML,
Sphinx doctrees and `sphinx-warnings.log`. `--output /path/to/output` selects another
output root; keep it outside `docs/`. Generated API pages under
`docs/api/generated/` and build outputs are ignored by Git. Edit the declaring
headers or authored guides, then regenerate. Generation is serialized within a
checkout because all output profiles share the generated API source directory.

The generator treats Sphinx warnings and unresolved references as errors, then
checks the public-header inventory, private-member exclusion, local links and
fragments, case-insensitive filename collisions, and source objects at the checked
revision. Successful extraction does not establish that every declaration has
useful prose, examples or a complete contract. Review public comments and the
[API index](../api/index.md) separately.

Derived pages link to the declaring base for inherited interfaces. Public nested
types have their own pages rather than repeated declarations in the outer class.
The reference adds Doxygen's `final`, `[[nodiscard]]` and template constraints as
declaration notes, and includes concept definitions. Each declaration page links
to its header at the recorded source revision. Consult that header for the full C++
declaration. Existing Mermaid fences are displayed as plain text until a diagram
renderer is selected; they are not rendered diagrams.

## Tool selection and reuse

[bootstrap.py](../../tools/docs/bootstrap.py) prefers a compatible installed
Doxygen and the Python packages in the invoking interpreter. Newer versions are
allowed for local work. Python selection checks the complete installed dependency
closure, version constraints, Python requirements and imports, rather than only
the five direct package versions.

| Tool | Minimum installed version |
| --- | --- |
| Doxygen | 1.18.0 |
| Sphinx | 8.2.3 |
| Breathe | 4.36.0 |
| Exhale | 0.3.7 |
| MyST Parser | 5.1.0 |
| Sphinx RTD Theme | 3.1.0 |

If installed tools do not qualify, bootstrap selects the exact versions in
[requirements.txt](../requirements.txt) and
[native-tools.json](../../tools/docs/native-tools.json). Python fallback uses an
isolated virtual environment, a pinned pip wheel and hash-checked binary wheels;
it does not install into the invoking interpreter. Doxygen fallback uses a
hash-checked official archive. A healthy cached environment is validated and
reused without running an installer.

The default shared cache is `cheryl-engine/docs-tools` beneath the platform's
user cache: `$XDG_CACHE_HOME` or `~/.cache` on Linux, `~/Library/Caches` on macOS,
and `%LOCALAPPDATA%` on Windows. Python cache identity includes the interpreter,
platform and requirements hash; native cache identity includes the archive hash.
Locks and readiness markers prevent concurrent setup from reusing a partial
installation. An interrupted setup is repaired on the next invocation.

Pinned Doxygen archives are provided for Linux x86_64, Windows x86_64, and macOS
x86_64/arm64. Other hosts need a compatible installed Doxygen; Python fallback
also requires matching binary wheels. The pinned Python lock has wheel metadata
coverage for CPython 3.11–3.14 on these platforms. Linux fallback has been exercised;
Windows and macOS native setup still require execution acceptance.

`status` reports tool readiness without installing anything. `setup` prepares tools
without generating documentation. Use `exec --force-pinned` for reproducible
generation, including CI and Read the Docs:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  python tools/docs/bootstrap.py exec --force-pinned -- python tools/docs/build.py
)
```

Set `CHERYL_DOCS_CACHE_DIR` or pass bootstrap `--cache-dir /path/to/cache` to choose
another shared cache. To select another interpreter, invoke bootstrap with that
interpreter. `build.py --doxygen /path/to/doxygen` supplies an explicit executable,
which must meet the minimum and takes precedence over automatic native tool
selection. Keep local overrides out of publication configuration.

## CMake entry points

The docs-only project needs CMake 3.28 or newer and Ninja in addition to Python.
It enables no compiler languages and does not discover engine dependencies:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S tools/docs -B build/documentation -G Ninja
  cmake --build build/documentation --target docs
)
```

This writes `build/documentation/docs/html/index.html`. The optional `docs-tools`
target prepares tools; the `docs` target also selects them, so running `docs-tools`
first is unnecessary. Neither target belongs to the default build.

For an engine build, configure `CHERYL_BUILD_DOCS=ON` and explicitly build `docs`.
The root project still has its usual compiler and dependency requirements; use the
[building guide](building.md) for that configuration. Both CMake entry points expose
`CHERYL_DOCS_PYTHON`, `CHERYL_DOCS_CACHE_DIR` and `CHERYL_DOCS_FORCE_PINNED`:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  cmake -S tools/docs -B build/documentation -G Ninja \
    -DCHERYL_DOCS_PYTHON=/path/to/python \
    -DCHERYL_DOCS_CACHE_DIR=/path/to/shared-cache \
    -DCHERYL_DOCS_FORCE_PINNED=ON
  cmake --build build/documentation --target docs
)
```

## Refreshing the pinned tools

Treat a tool upgrade as a coherent change to the fallback environment, rather than
changing one direct version and accepting whatever an installer resolves:

1. Resolve the full dependency closure from official PyPI release metadata,
   including platform and Python markers. Pin every required package and the
   installer; include SHA-256 hashes for the accepted wheel variants. Keep binary
   wheels required, and verify Python requirements and wheel availability across
   the supported Python/platform combinations.
2. Update `native-tools.json` when changing Doxygen or the pinned pip wheel. Obtain
   archive URLs and digests from the [official Doxygen releases](https://github.com/doxygen/doxygen/releases)
   and [PyPI](https://pypi.org/), and keep platform mappings explicit. Update the
   bootstrap minima only when the supported local baseline changes; fallback pins
   and minimum installed versions serve different purposes.
3. With execution authorized, generate using `--force-pinned`, check the output,
   and confirm warm-cache reuse. Review API signatures and public/private scope
   when changing Doxygen, Breathe or Exhale. A new requirements hash selects a new
   cache identity; old caches do not need to be overwritten.

## Publishing contract

The publishing configuration is
[.readthedocs.yaml](../../.readthedocs.yaml),
[documentation.yml](../../.github/workflows/documentation.yml) and
[sync-docs-stable.py](../../.github/scripts/sync-docs-stable.py). Hosted publication
still requires the account and repository setup below. Both `develop` and `main`
must contain the engine headers and documentation implementation before their
respective sites can build.

GitHub Actions generates and checks HTML for pull requests, pushes to `develop`
and `main`, `v*` tags, and manual runs. It retains checked HTML and build diagnostics
as downloadable artifacts for 30 days. Read the Docs independently clones the
selected Git ref, runs the same pinned generator, and hosts the resulting HTML.
Actions artifacts are not the website's source and do not need an upload token for
Read the Docs. Engine compilation belongs to the existing code CI; the documentation
job does not compile examples.

The Read the Docs configuration uses Ubuntu 24.04 and Python 3.13, with one custom
`build.jobs.build.html` command:

```yaml
version: 2
build:
  os: ubuntu-24.04
  tools:
    python: "3.13"
  jobs:
    build:
      html:
        - python tools/docs/bootstrap.py exec --force-pinned -- python tools/docs/build.py --output "$READTHEDOCS_OUTPUT"
```

Keep `sphinx`, `mkdocs` and a separate Python installation step out of this custom
configuration: bootstrap owns setup and generation. Read the Docs publishes the
`html/` directory beneath `$READTHEDOCS_OUTPUT`. Its
[custom build documentation](https://docs.readthedocs.com/platform/stable/build-customization.html)
and [configuration reference](https://docs.readthedocs.com/platform/stable/config-file/v2.html#build-jobs)
describe this contract. PDF and other builders are outside the current configuration.

### Version policy

| Website version | Git source | Policy |
| --- | --- | --- |
| `latest` | `develop` | Current development documentation. |
| `stable` | Explicit `stable` branch | Fast-forward mirror of a checked commit on `main`. |
| Release version, such as `vX.Y.Z` | Matching release tag | Historical documentation, activated individually or by an agreed tag rule. |

Set the Read the Docs default branch to `develop`. An explicit Git branch named
`stable` supplies the stable version instead of Read the Docs automatically choosing
the greatest stable semantic version tag. Do not create a conflicting `stable` tag.
Activate desired historical tags and leave them visible for the version selector;
tags predating the documentation tooling cannot generate a site without a separate
backport policy. See [Read the Docs versions](https://docs.readthedocs.com/platform/stable/versions.html).

The stable sync job is disabled unless the repository variable
`CHERYL_DOCS_PUBLISH_STABLE` equals the literal string `true`. Enable it only after
approval, the documentation merge into `main`, and account setup. The job must run
only for canonical-repository pushes to `main`, after documentation checks succeed;
it must never check out pull-request code with write credentials. Serialize stable
updates and compare the checked revision with the live `main` ref before updating.
Create `stable` if absent, otherwise fast-forward it without force. Reject divergence
and respect repository rules; do not delete, reset or force-update the branch to
bypass a rejection. GitHub's [ref API](https://docs.github.com/en/rest/git/refs#update-a-reference)
supports the required non-forced update.

Read the Docs builds `latest` directly from `develop`; that build is independent of
the Actions result. The stable mirror advances only after the documentation job
succeeds. Branch protection and release policy determine which engine checks must
pass before a change reaches `main`.

### First account and site setup

1. Merge the documentation configuration into `develop`. Create an account at
   [Read the Docs Community](https://app.readthedocs.org/), install its GitHub App
   with access to `cppcooper/cheryl-engine`, and import that repository. Importing
   requires repository administration access. The App subscribes to the required
   events, so no additional manual webhook is needed. If the project was imported
   manually, connect the repository in project settings. See the
   [Git integration instructions](https://docs.readthedocs.com/platform/stable/reference/git-integration.html#adding-a-project-from-a-repository).
2. Record the actual project slug and public URL returned by Read the Docs; do not
   assume the slug from the repository name. Set its default branch to `develop`,
   activate `latest`, and check that version using the publication checks below.
3. Merge the approved release into `main`. Enable `CHERYL_DOCS_PUBLISH_STABLE=true`
   only when stable publication is approved. Enabling the variable does not
   trigger a workflow, and manual dispatch runs only the documentation checks.
   The next qualifying `main` push creates or advances the `stable` branch; let
   Read the Docs sync versions, then activate `stable`. Trigger a build to refresh
   versions if it has not appeared yet.
4. After a successful stable build and revision check, choose `stable` as the
   project's default website version. Activate existing release tags manually;
   a future-tag activation rule does not replace that initial selection.

### Publication verification and recovery

Every full build writes `html/revision.json`. Its
`source_revision` is the full checked Git commit, `source_ref` identifies the build
ref, and `dirty` records whether the source checkout differs from that commit.
The marker also records selected tool versions and lock/manifest hashes, without
machine-specific executable or cache paths. Source links must use the same full
commit; an overridden revision must match Git `HEAD`.

For the first site and each publication investigation, fetch `revision.json` from
the actual version URL, for example `<version-url>/revision.json`. Compare
`source_revision` exactly with the expected commit and the Read the Docs build's
reported commit; a successful build status or plausible page content is insufficient.
For `stable`, also compare the Git `stable` and intended checked `main` revisions.
Publication requires a clean source marker, successful internal links and fragments,
and Git source links resolving to objects at that commit.

First-site acceptance also includes opening the manual and API entry points,
following an inherited base link and a declaring-header link, checking navigation
and search, and switching between `latest` and `stable` once both are active.
Check historical-version selection when a release tag containing the documentation
tooling is available; do not count that coverage before such a tag exists. Confirm
the GitHub App triggers the expected branch rebuilds. Local HTML checks do not
establish that the hosted version selector, search or webhook delivery works.

To pause stable updates, set `CHERYL_DOCS_PUBLISH_STABLE` to `false`. Correct a bad
publication with a new reviewed commit on `main`, including a revert when restoring
earlier content, then let the normal fast-forward path rebuild `stable`. Do not move
the stable branch backward. If a ref diverges or repository rules reject an update,
inspect the refs and resolve the cause before retrying; do not force the mirror.
After recovery, repeat the exact revision and page checks against the public URL.
