# CI and SDK artifacts

Use the hosted workflows to validate selected compositions, collect prebuilt SDKs
and maintain build caches. [Building Cheryl](building.md) owns local configuration
and SDK production; [test selection](testing.md) owns runner and fixture choices.

## GitHub CI and SDK artifacts

[ci.yml](../../.github/workflows/ci.yml) validates pushes to `develop`/`main`, pull
requests targeting either branch and manual dispatches. Each job checks out recorded
recursive submodules. Actions use commit pins; Windows uses the manifest's pinned
vcpkg baseline. Builds use Ninja and available CPU parallelism.

| Job | Host/compiler/configuration | Selection |
| --- | --- | --- |
| `linux-gcc-hwloc` | Ubuntu 24.04 x64, GCC 14 Release | Headless Engine, hwloc and JSON. |
| `linux-gcc-neutral` | Ubuntu 24.04 x64, GCC 14 Release | Headless Engine and JSON, hwloc omitted. |
| `linux-clang-debug` | Ubuntu 24.04 x64, Clang 18 Debug | Headless Engine, hwloc/JSON, ASan/UBSan. |
| `windows-msvc-hwloc` | Windows 2025 x64, MSVC RelWithDebInfo | Headless Engine and hwloc/JSON with vcpkg native libraries. |
| `linux-modules` | Ubuntu 24.04 x64, GCC 14 Release | Native GLFW/input, OpenGL, TGUI, RmlUi, miniaudio, hwloc/JSON and SDK. |
| `windows-modules` | Windows 2025 x64, MSVC RelWithDebInfo | The same selected modules/helpers and SDK, with bundled native dependencies. |
| `linux-modules-shared` | Ubuntu 24.04 x64, GCC 14 Release | The modules/SDK composition with shared Cheryl libraries and owned dependencies. |
| `windows-modules-shared` | Windows 2025 x64, MSVC RelWithDebInfo | The shared modules/SDK composition, including DLL runtime discovery. |

[run.py](../../.github/ci/run.py) configures explicit owners/options/build targets,
runs each focused owner prefix once and executes independent consumers. Modules
jobs compile the demo for composition coverage but never launch or package it.
Linux jobs provide TrueType/CFF allocation fixtures. Missing fixtures and
platform-specific skips are reported as unaccepted coverage. Native OpenGL, native
hwloc binding acceptance, actual audio devices, desktop/controller QA and the
Debug terminal remain separate from this focused CI selection.

Modules jobs package and verify an extracted SDK on every validation run.
[verify_sdk.py](../../.github/ci/verify_sdk.py) checks the checksum and absence of
producer paths, then builds independent consumers/header probes using only installed
Cheryl libraries. Test fixture sources/fonts come from the checkout; Cheryl and
module implementation sources are not rebuilt. Use a fresh consumer build/extraction
directory for each verification. See
[repeating SDK checks](architecture-validation.md#installed-sdk-consumers).

Successful main-push modules jobs retain SDK candidates for three days. After all
headless and modules jobs succeed, the collection job verifies checksums and uploads
`cheryl-development-sdks-<commit>` for 90 days. Test/configuration reports are retained
for 14 days. These are workflow artifacts; the workflow creates no GitHub release,
uploads no release assets and publishes no demo package. Release publication is a
separate future decision.

The first main-push artifact collection remains
[TR20](../testing-requests.md#tr20-automated-main-push-sdk-collection); collection is
intentionally skipped on PR, `develop` and manual runs.

### Actions cache retention

Compiler and dependency caches speed up subsequent CI runs. They are separate
from the SDK and diagnostic artifacts above, and from each other:

| Cache | Key family | Contents and ownership |
| --- | --- | --- |
| Compiler results | `ccache-v2` | Main, develop and PR layers, described below. |
| X: build dependencies | `build-deps-v2` | Separate main/develop submodule Git data and Windows vcpkg package archives/downloads. PRs read their base branch's X. |
| Y: Actions environment | `actions-env-cache-v2` | Shared tools, Linux package downloads, Python wheels/downloads and prepared documentation tools, published by main. |

Each compiler matrix configuration has separate ccache layers:

| Build | Readable layers | Writable layer saved after building |
| --- | --- | --- |
| `main` | Main baseline A | A |
| `develop` | A + develop additions B | B |
| PR targeting `develop` | A + B + PR additions C | C |
| PR targeting `main` | A + C | C |

[restore-compiler-cache](../../.github/actions/restore-compiler-cache/action.yml)
restores the available layers into separate directories.
[compiler_cache.py](../../.github/ci/compiler_cache.py) configures ccache to check
the current layer first, followed by its parents, and write compilation results
only into the current layer. Parent hits stay in the parent layers. Statistics
live outside the archived data. Before saving, the helper removes compiled
results already available under the same cache key in a parent. It preserves
distinct lookup metadata, then trims the writable layer to 1 GiB.
An unchanged layer is not uploaded again. A layer that becomes empty replaces its
previous snapshot with a small empty-layer marker, allowing cleanup to remove
the older duplicated data.

This hierarchy requires `main` to be GitHub's default branch. A PR can read its
own cache, its base branch's cache and the default branch's cache; a PR targeting
`main` cannot read `develop`'s cache. Manual feature-branch builds use their own
writable layer and the main baseline. These are
[GitHub cache scope restrictions](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching#restrictions-for-accessing-a-cache).
Missing parents cause normal cache misses until their builds populate them.
The `ccache-v2` keys distinguish this file-backend layout from the previous
ordinary local-cache archives; the first build of each configuration seeds the
new format. Compiler layers and Windows binary dependency snapshots use new
run-specific keys because saved GitHub caches are immutable.

Cache X's source key hashes `.gitmodules` and the recorded submodule commits.
Updating a submodule selects a new source snapshot automatically; an older X can
seed Git objects before checkout fetches the requested commits. The binary
component follows the vcpkg manifest and runner image, with vcpkg validating each
package's binary compatibility when using a fallback snapshot. Vendored libraries
remain ordinary CMake targets and reuse their compiler results through ccache.
Only main/develop push and manual runs publish X; PRs fetch or build missing
dependencies locally.

To update Y, change package/tool selections in
[environment.json](../../.github/ci/environment.json), generator packages in
[requirements-modules.txt](../../.github/ci/requirements-modules.txt), or the
documentation tool manifests. Bump `cache_version` from `2` to `3` when revising
the Actions environment or forcing a refresh; this changes the family to
`actions-env-cache-v3`. Environment inputs are also hashed so package changes
cannot silently reuse a mismatched key. Y has separate immutable components for
each OS/architecture, runner image and environment composition. Resolved Linux
package versions, Python ABI and tool manifests further identify their payloads;
submodule commits and run IDs do not affect Y.

Every branch restores Y, while only trusted main push/manual runs publish it.
Develop and PR changes to the environment install normally on a cache miss;
main publishes the updated Y after those changes reach it. Package installers
still run on each fresh runner; cached downloads avoid fetching the same packages
again. Documentation tooling is prepared and cached before document generation,
so a later documentation failure does not discard the prepared environment.

[cache-maintenance.yml](../../.github/workflows/cache-maintenance.yml) runs after
the build or documentation workflow finishes and daily at 04:23 UTC. It removes
caches belonging to closed pull requests, keeps the newest mutable compiler and
dependency snapshot per branch, configuration and cache version, and trims older caches
toward an 8 GiB repository budget. Active workflow refs and caches created or
accessed within the last 15 minutes are protected. After removing redundant
snapshots, budget eviction removes ordinary old caches first, then the latest
open-PR compiler/dependency snapshots, then develop snapshots, and finally main
snapshots. Known default-branch tooling caches have the same priority as develop
layers. Each tier uses oldest access first. The newest X source snapshot per
branch and runner receives that branch's priority; older submodule pins remain
ordinary eviction candidates. Old-format compiler snapshots lose their branch
priority once the same branch and configuration has a new-format replacement.
These priorities preserve useful branch caches where space allows; the budget
can still require evicting a current layer. The budget is a target: concurrent
uploads and protected caches can temporarily keep usage above it.
Cleanup deletes cached build inputs, without deleting SDK artifacts or changing
build results; an evicted cache is regenerated when next needed.

To inspect or apply cleanup, open **Actions → Maintain Actions caches → Run
workflow** after this workflow has reached the repository's default branch.
Leave **dry_run** enabled to preview cache IDs, keys, refs, sizes and deletion
reasons. Optionally enter a closed PR number in **closed_pr** to restrict the
operation to that PR's caches. Review the run's report, then dispatch again with
**dry_run** disabled to apply the current plan. Automatic runs apply cleanup;
they execute the default branch's script without loading PR code, artifacts or
cached executables.

GitHub also [expires unused caches and evicts old entries at its storage
limit](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching#usage-limits-and-eviction-policy).
The maintenance policy removes redundant snapshots before that limit causes
frequent replacement of useful caches.

### Hosted Windows coverage limits

Both Windows profiles leave these focused cases unaccepted:

| Cases | Missing prerequisite and next action |
| --- | --- |
| `font_bake.real_nested_allocation_scope`, `font_bake.real_cff_alloc_failure` | The workflow supplies the bundled TrueType face but no CFF OpenType fixture on Windows. Supply `CHERYL_STB_ALLOCATION_CFF` with a suitable face, retain the TrueType fixture for nested scopes, and rerun these cases on hosted Windows. |
| `logging.config_reopen` | The test needs a temporary directory on the checkout's filesystem root so it can use a relative log path. Configure the hosted Windows temporary directory on that drive, then rerun the case. |

The [font fixture requirements](architecture-validation.md#native-and-fixture-checks)
define suitable faces. A green job does not accept these skipped cases; successful
local MSVC/Wine checks do not replace the missing hosted execution. These limits
are separate from SDK collection and desktop/GPU/device acceptance.
