# Publishing documentation

Publish the generated manual and API as a versioned Read the Docs site. Prepare
the artifact with [documentation generation](documentation.md), then use the
account, version and verification procedures below. Hosted setup requires the
repository/account permissions stated in the setup procedure.

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
Both publication paths request full generation with `--clean`.
Actions artifacts are not the website's source and do not need an upload token for
Read the Docs. Engine compilation belongs to the existing code CI; the documentation
job does not compile examples.

Direct pushes to other branches do not schedule these validation jobs. Pull
requests targeting `develop` or `main` still run them. Keep workflow definitions
valid on every pushed branch: GitHub creates a failed run for each new commit
containing an invalid workflow, even on a branch excluded by its filters. Changing
branch filters or job conditions does not suppress those configuration errors;
correct the invalid YAML or expression. See
[GitHub's workflow-log guidance](https://docs.github.com/en/actions/how-tos/monitor-workflows/use-workflow-run-logs).

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
        - python docs/generation/bootstrap.py exec --force-pinned -- python docs/generation/build.py --clean --output "$READTHEDOCS_OUTPUT"
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

Every successful HTML generation writes `html/revision.json`. Its
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
