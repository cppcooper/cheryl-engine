> **Parked by user direction, 2026-10-23.** See [parked.md](parked.md).
> U4 is an unverified interrupted draft; earlier U3 checks do not accept it.

# SETTINGS-U3 handoff and next unit

U3 implements only the app-private `ScoreInput` owner and move-only
Candidate/Prepared protocol in SET-005, based on accepted U2 commit
`2d9bdd873ab6ab23ab032465f76d1b2ed9cd8f0d`. The small caller and precise transition
contracts are in [agent notes](agent-notes.md); [decisions](decisions.md) carries
the matching IDs. Game's direct score mapping, player/non-score wiring, UI,
runtime, Engine APIs and settings value/file implementation are unchanged.

The Linux Release selection passed **103/103, with no skips or failures**:
21 new `tutorial_score_input.*` cases plus all 82 accepted U2 cases. These comprise
55 Engine input/context/publication cases, 22 settings cases, five gameplay cases,
and the 21 score-owner cases. This accepts the private CPU protocol, not settings
UI, runtime Apply, or gameplay admission.

## Executed commands and evidence

Reuse `build/settings-u1-release`: Linux Release, GNU C++ 16.2.1, the pinned local
dependencies, and `CMAKE_POLICY_VERSION_MINIMUM=3.5`. No flags or pins changed.
Adding helper/case source registration triggered normal CMake regeneration inside
the incremental build. Only `tests-tutorial-game` needed rebuilding; the existing
Engine runner/library remain compatible because U3 changes no Engine source/API.
Helper source is registered for both app and cases, with existing JSON includes
still private. The graphical app and TGUI were not built or run.

Every build/native check used the shared wrapper with one job. Complete output is
retained in clone-local ignored `build/settings-u1-release/settings-u3-logs/`.
A queued command keeps its existing session; actual exit 75 requires the exact
refusal to be recorded and an idle checkpoint without an unchanged retry. No
resource refusal occurred here.

Build, exit 0:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake --build build/settings-u1-release \
  --target tests-tutorial-game --parallel 1 \
  > build/settings-u1-release/settings-u3-logs/build.log 2>&1
```

Configuration/regeneration completed. It repeated the known CMP0170 warning:
`FETCHCONTENT_FULLY_DISCONNECTED` expects the absent build-local source directory
`build/settings-u1-release/_deps/hidapi-src` to be populated. The accepted host
HIDAPI configuration remains available; this did not block the selected target.
There were no compiler diagnostics from the new sources. No dependency download,
installation, font substitution, pin change, or extra configuration was attempted.
The complete configure/build diagnostic and stack remain in `build.log`.

Discovery, exit 0:

```sh
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/settings-u1-release -N \
  -R '^(tests-engine\.(action_contexts|input_bindings|tick_input|input_state|input_contract|input_routing|input_capture|polling_backlog)\.|tests-tutorial-game\.(tutorial_score_input|tutorial_settings|tutorial_game)\.)' \
  --no-tests=error --parallel 1 \
  > build/settings-u1-release/settings-u3-logs/discovery.log 2>&1
```

A read-only Python check parsed actual registered names, required all 21 named
score-owner cases and exact inclusion of the 82 names in the accepted U2 discovery
log, and asserted the total of 103. `discovery-counts.json` retains every group
count. Thus a wrong selection could not pass by selecting only one group.

Execution, exit 0:

```sh
/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir build/settings-u1-release \
  --output-on-failure \
  -R '^(tests-engine\.(action_contexts|input_bindings|tick_input|input_state|input_contract|input_routing|input_capture|polling_backlog)\.|tests-tutorial-game\.(tutorial_score_input|tutorial_settings|tutorial_game)\.)' \
  --no-tests=error --parallel 1 \
  > build/settings-u1-release/settings-u3-logs/tests.log 2>&1
```

CTest reported `100% tests passed out of 103`, real time 0.95 s. A read-only check
compared every discovered name with every executed `Passed` name, checked the
summary and rejected skipped/not-run/failed markers. `verification-summary.json`
records the exit codes/counts. Formatting, internal links, stable decision IDs,
40–80-word compact entries and staged whitespace are also reviewed. No GUI,
documentation generation, real device, subagent or broad campaign was run.

Accepted earlier evidence remains in Git and clone-local logs: U1 at
`e4529aa9229334433a135b51598ebb50c11b8f4f` passed 26 settings/gameplay cases; U2 at
`2d9bdd873ab6ab23ab032465f76d1b2ed9cd8f0d` passed 82 cases in
`build/settings-u1-release/settings-u2-logs/`. Its committed `next.md` also retains
the original configure command and resolved prerequisite history. Current U3
counts above supersede the earlier per-run counts without inventing new coverage.

## Observed behavior, choices, and limits

- Initial and replacement readiness require namespace membership in `tick.polls()`.
  Inactive initial installation and attachment during an open poll are covered. Baseline membership alone cannot establish readiness.
  Witnessed active evidence latches per lease; candidate evidence additionally
  belongs to its exact generation. Empty later ticks may retain existing evidence.
- The helper reads only the active namespace. Tests show held-state seeding,
  old/candidate coexistence without combined score reads, explicit unchanged-key
  requests, and independent direct/player mappings with overlapping semantic IDs.
  Old context final release and later retirement remain observable after adoption.
- Stage validates through the sole U1 validator. It rejects another pending stage;
  an idle unchanged-key stage returns `nullopt` without a new lease. Changing
  generation explicitly cancels the prior candidate/prepared request as Stale;
  reusing a generation value never revives it. Callers must report every generation
  change through `set_generation`.
- One move-only token strongly owns a private replacement record/lease; the helper
  holds only its weak association. One extra bookkeeping allocation is accepted
  for replacement preparation. Token drop, cancel, owner destruction and move
  assignment release the appropriate request without extending owner lifetime.
  Wrong-owner attempts do not consume a valid token or mutate another owner.
- Tests cover invalid keys, pending/prepared rejection, stale generations,
  cancellation/drop before and after publication, clear/shutdown despite retained
  membership, active/pending owner moves, candidate/prepared moves, and invalid or
  foreign adoption. Closed input necessarily invalidates existing leases: stage
  rejects the surviving helper locally; construction separately exercises the
  closed-endpoint attachment failure. No allocator failure or RAM exhaustion was
  injected. Attachment's strong active-state preservation also follows source
  ordering: the pending slot changes only after successful attachment.
- Preparation is fallible and preserves active ownership. Successful adoption is
  declared `noexcept` and uses only guarded lease/primitive moves and weak reset;
  source review establishes that it allocates nothing. Static assertions and real
  adoption/move/retirement cases check the boundary. Invalidated/stale/cancelled or
  wrong-owner prepared tokens return false without changing active ownership.
- Confidence is high in this tested CPU protocol; names and the weak-slot/token
  representation remain low-cost private engineering choices before integration.
  They are not new claims about v3 user preference. Preparation/adoption is not a
  transaction across concurrent input teardown. The caller must serialize owner
  mutation and exclude teardown between the valid preparation and adoption.
- Readiness is not neutral gameplay admission. The seeded-edge test demonstrates
  why future SET-003 modal/release-watermark/rearming policy is still required.
  Real device, GUI, sequential/concurrent Game integration, file/owner composition,
  power-loss durability and other-platform acceptance remain outside this unit.

No U3 blocker remains. The helper is intentionally unintegrated with Game and
persistence; those boundaries require the next coordinator-selected assignment.

## One proposed next implementation unit, not started

U4 should compose the typed validator, `SettingsFile`, and `ScoreInput` into a
bounded CPU Apply transaction before adding widgets or changing Game wiring.
Show its small caller and make generation/cancellation and the owner/teardown
precondition explicit. Prepare a valid complete value and optional score token,
wait through real consumed observations, validate preparation, save once, then
perform only nonthrowing ownership/value adoption. An unchanged binding retains
its active lease. No modal gate, watermark/rearm implementation, Workers, new
Engine API or recurring work is activated by this handoff.

Checks should inject each existing file failure while asserting exact predecessor
bytes, active settings/key/context and retained editable candidate; cover unchanged
keys, invalid values, stale/cancelled candidates and input invalidation before
saving; and verify a successful save/reload plus old-context retirement. Exclude
concurrent teardown from the transaction promise and define the outcome if runtime
shutdown intervenes. Later UI/admission integration still needs its own selection.
The coordinator selects this unit separately. Stop after the U3 checkpoint;
checkpoint transport belongs to the coordinator publisher.
