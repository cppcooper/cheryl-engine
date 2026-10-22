# Generate and verify documentation

Use this guide when generation, generated-page inspection or stale-output diagnosis
is part of the authorized task. Read the [shared workflow](workflow.md) and exact
[operation permissions](testing.md#verification-and-resource-use). Full generation
requires authorization; reuse authorization already given for the relevant work.

The [published tooling guide](../site/development/documentation.md) owns commands,
cache invalidation, tool versions, CMake entry points and output selection. Use
[API documentation](api-documentation.md) for declaration association and
[documentation workflow](documentation.md) for content correctness. Generation
success alone establishes neither.

## Locate the current inputs and output

1. Check the current checkout, branch, Git status and affected diffs. Distinguish
   authored source, extraction inputs, generated API sources and HTML.
2. Use `docs/site/` as the manual source and `docs/site/generation/` for the
   generator/configuration. API inputs come from the generator's explicit inventory.
3. For normal local delivery, generate into
   `docs/site/generation/generated/html/`. The entry is `index.html`. Use another
   output profile only when it serves the requested verification; it does not
   replace the normal local site promised to the user.
4. When diagnosing stale content, inspect the browser path and that output's
   `revision.json`. Old folders, other checkouts and cached tabs can show a valid
   older site. A file's existence does not establish current generation.

## Run the relevant generation

Use the existing pinned/cached tools through the repository wrapper. Inspect
readiness before installing or downloading anything; reuse compatible tools and
caches. Preserve active user invocations and bound agent-run parallelism.
Follow the tooling guide's normal generation, extraction-only and clean-rebuild
choices based on the changed input or observed failure. Do not bypass the owning
wrapper or change unrelated build configurations to make output appear current.

For authorized normal local generation, from a Git working tree:

```sh
(
  set -e
  cd "$(git rev-parse --show-toplevel)"
  ./generate-docs.sh --jobs 4
)
```

Agent invocations may use reduced process priority; user-run commands retain
normal priority and appropriate available parallelism. The `--jobs` argument
bounds Sphinx workers, not an Engine build.

## Verify the delivered artifact

- Require successful generation and its local link/source checks. Inspect warnings
  and failures; do not present an older HTML file left behind as a successful rerun.
- Inspect representative changed page bodies. For API work, follow comments from
  source through XML into visible type/member prose, correct signatures and guide
  links. The presence of an identifier or anchor alone does not prove prose rendered.
- Compare `revision.json` with the intended source revision and dirty status. For a
  final committed delivery, generate/check that revision and confirm the normal
  output path. A dirty preview remains a preview and must be described accordingly.
- Check old bookmarks and duplicate generated roots when the user's observation
  disagrees with the inspected output. Link the exact current page and explain any
  identified stale copy. Preserve user outputs; remove or replace them only within
  an authorized cleanup task.
- Report the actual generated entry, revision, inspected content and checks.
  Keep source review, example execution and hosted publication acceptance distinct.
  Local generation does not publish or verify the hosted site.
