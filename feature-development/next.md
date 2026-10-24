# U6 reusable bounded Engine inspection

STORYTELLING-U6 starts from clean accepted U5
`24f9e04b2e4f912be6e0c56ec2cca009b971c16e`. The public CPU operation, owned
report, typed budgets, thin private adapter and independent actual-Engine consumer
are implemented. **112/112** focused cases passed: **14 new plus all 98 retained**
(45 Engine cases, 3 separate consumer cases, 64 app cases), no skips/unrun cases.
Final added boundary assertions passed **2/2** focused rechecks. All **54** complete
child blocks match U5 **byte for byte**, including arguments, stdin, stdout,
stderr and immediate statuses (21 exit 0 / 15 exit 2 / 18 exit 3).
No ASan/UBSan diagnostics or resource admission refusals occurred. Assigned checks
are complete, ready for coordinator review; no next unit is started.

## U6 caller and implemented boundary

The self-contained public `cheryl/core/game-framework/dialogue-inspection.h` owns
InspectionLimits, maximum_inspection_limits, InspectionError, InspectionUsage,
InspectionReport and one CE_API operation:

```cpp
#include <cheryl/core/game-framework/dialogue-inspection.h>
auto result = CE::Dialogue::inspect(*definition, CE::Dialogue::InspectionLimits{});
if (result) {
    const auto& report = *result; // independent owned IDs/counts/lists/usage
} else {
    const CE::Dialogue::InspectionError refusal = result.error();
    // Handle the typed refusal; there is no partial report.
}
```

The caller retains a successfully compiled Definition only for the call. Returned
IDs/lists survive all draft/definition owners, with no retained input reference.
Reports preserve authored order, actual compiled entry, explicit terminal kinds,
all structural edges and empty terminal paths. Conditions/effects are never
invoked. Cycles, self/parallel/conditional edges and terminal-free components remain
valid. Static topology is advisory, not gameplay/history/termination/completion
proof. Definition/Draft/Session and application operation semantics are unchanged.

Matching decisions ST-013/014 are updated and ST-015 adds actual Engine ownership/
consumer policy. Default budgets are 256 nodes, 2048 choices, 131072 repeated ID
bytes, 1048576 payload bytes and 134217728 work units. Hard request ceilings are
4096 / 65536 / 4194304 / 16777216 / 1073741824 respectively. Zero means no allowance;
any request above a ceiling, including size_t-max, returns InvalidLimits before
traversal/allocation. Other typed refusals distinguish node, choice, copied-ID,
storage and work budgets. Larger inputs are supported under raised budgets; tests
execute 257 nodes, 2049 edges and 256-byte IDs without importing private JSON caps.

One contiguous actual-size scratch allocation supports iterative forward/reverse
CSR traversal. Its index payload is `(2E + 6N + 3)*sizeof(size_t)`. With S output
node-ID slots and B repeated copied ID bytes, peak charged payload is
`scratch + sizeof(InspectionReport) + S*sizeof(NodeId) + B + S+2`. Counts/bytes/
products and minimum payload are checked before scratch allocation, exact final
payload/copy work before report allocation. Work charges actual row/edge visits,
scratch index passes, lookup candidates/compared bytes, report appends and copied
ID bytes/terminators. Exact formulas, unit meanings and tradeoffs are in ST-014
and the public declaration. Storage excludes allocator metadata/
rounding, fixed stack and input; work excludes fixed bookkeeping and allocator
internals. These are explicit payload/work budgets, not resident-memory or latency
guarantees. Allocation exceptions propagate; U6 does not inject allocator failure
or deliberately exhaust memory. Prior values remain untouched on refusal/exception.

Engine's existing source glob owns the new `.cpp`; no alternate archive/copy,
export macro override, JSON/path/format/session or generic graph API was added.
The private report header aliases InspectionReport; its adapter retains only the
legacy ordered cap/message preflight and delegates topology to Engine. Formatter
bytes, loader/schema/allowlist, built-in/loaded interactive modes, paired saves,
fresh-save guarantees and dependency pins remain compatible. Engine linkage still
has its existing dependency closure; CPU-only is not dependency-free packaging.

## Actual U6 native and linkage checks

The preserved clone-local GNU/Linux Debug shared-Engine profile, policy minimum
3.5, prior OFF selections, pins and incremental outputs were reused. Every
configure/build/native metadata/test check ran through the shared one-job wrapper.
The adaptive monitor controls native admission/caps; no swap and 4 GiB host floor
remain in force. No monitor/global/sanitizer build setting changed. Invocation-only
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1 remain
ST-008's accommodation, with ASan/UBSan instrumentation retained. Leak coverage is
**unverified**. No native child reacquires the lock; child consoles inherit the
wrapped test envelope. No cancellation/relaunch, exit 75 or unchanged retry occurred.

Actual primary commands from clone root (all exit **0**, full logs below):

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine tests-dialogue-inspection dialogue-console tests-dialogue-console --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.)'
```

Final review added exact maximum-allowance acceptance and a post-traversal storage
refusal assertion. The same incremental build command ran again (exit **0**), then
only those changed cases reran (exit **0**, 2/2, 0.21 seconds):

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^tests-engine\.dialogue_inspection\.(storage|invalid_limits)$'
```

A supplemental wrapped CMake script executes nm/readelf serially and captures
complete outputs/immediate statuses. Exact invocation:

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake \
  -DINSPECTION_BUILD_DIR=/mnt/projects/cheryl-features/projects/storytelling/builds/storytelling-u1-debug \
  -P builds/storytelling-u1-debug/u6-linkage-audit.cmake
```

Its native tools all exit **0**: `nm -D -C --defined-only .../libcheryl-engine.so`,
`nm -D -C --undefined-only .../tests-dialogue-inspection`, and `readelf -d` on that
consumer. They show `T CE::Dialogue::inspect(Definition const&, InspectionLimits const&)`,
consumer `U` import, and NEEDED libcheryl-engine.so.4. The script itself exited
**1** only at its final matcher: `RegularExpression::compile(): Invalid range in [].`
The initial script/log/exit remain intact. Replacing that expression with literal
`string(FIND)` in `u6-linkage-audit-2.cmake` and executing the same wrapped command
with that `-P` path exits **0**; it rereads the saved successful tool outputs,
without repeating native metadata commands or changing implementation/check flags.

| Complete clone-local files under `builds/storytelling-u1-debug/` | Observed result |
| --- | --- |
| `u6-configure.log`, `.exit` | 0; retained profile/options/pins. |
| `u6-build-1.log`, `.exit`; `u6-build-2.log`, `.exit` | 0; actual shared Engine compiles/owns source, public consumer and private adapter compile/link/discover. Second build adds final boundary assertions/API comment clarification only. |
| `u6-registration.log`, `.exit`; `u6-registration-audit.log` | 0; exactly 112 cases, source names/order match, all 98 previous names/order retained, 14 new. |
| `u6-ctest-1.log`, `.exit`; `u6-ctest-details-1.log` | 0; 112/112 passed, 14.07 seconds. Full RUN/OK/CTest and 54 complete child records retained. |
| `u6-boundaries-ctest-2.log`, `.exit`; `u6-boundaries-details-2.log` | 0; 2/2 changed boundary cases passed, no skips or sanitizer diagnostics. |
| `u6-engine-exports.log`, `.exit`; `u6-consumer-imports.log`, `.exit`; `u6-consumer-needed.log`, `.exit` | All 0; real shared export/import/NEEDED proof. |
| `u6-linkage-audit.cmake`, `.log`, `.exit`; `u6-linkage-audit-2.cmake`, `.log`, `.exit` | Initial matcher 1 (invalid escaped regex), corrected read-only matcher 0; both complete attempts retained. |
| `u6-boundary-rules.log` | Real Engine source object has CE_BUILDING_ENGINE; independent consumer lacks private/app/JSON includes and export/static macro patches. Both retain ASan/UBSan. |
| `u6-results-audit.log` | Discovery/execution counts/names, 98 regressions, no skips/sanitizer errors, all 54 full child blocks byte-identical to U5; resolved audit failure distinguished. |

The header-first separate consumer executes values, ownership and typed errors
across the real shared boundary; its Linux dladdr assertion confirms the function
owner equals the Engine target file. Three consumer checks are registered through
the existing Engine test helper, linking Cheryl::Engine without a private source
copy or JSON/app includes. Compile/link/export evidence complements actual returned
results; an unused successful link is not counted as acceptance. All logs, audit
scripts, disposable fixtures and immediate-status files remain ignored/clone-local.

| New passed cases | Observable checks |
| --- | --- |
| dialogue_inspection.literal, terminal_free, parallel_self | Independent literal branch/rejoin, nonfirst entry/authored order, cycles/self/parallel/conditional/effect-key edges, unreachable terminal/closed components, zero-terminal structure, terminal empty path; no handlers/sessions exist. |
| ownership, copied_ids | Draft mutation/destruction, definition weak owner expires, independent report copies, exact repeated ID budget/+1 refusal and old report stability; Engine accepts a 256-byte ID. |
| nodes_choices, larger | Exact node/edge and zero-edge budgets, one-short refusals, 257 nodes/2049 edges accepted under raised limits. |
| storage, work, invalid_limits, compared_bytes | Exact payload/work budget and one-short/zero refusals; final late storage refusal; maximum valid allowances and every field above hard ceiling/size_t-max; equal-length late byte comparisons cost more than early mismatches. No huge allocations/OOM test. |
| dialogue_inspection_consumer.public_header, retained, typed_limits | Self-contained header first, actual shared symbol owner/export/import, executed values, retained independent IDs and typed work/invalid-limit refusals with old report unchanged. |

## U6 next action and remaining limits

No unresolved assigned blocker remains. Next concrete action: coordinator review
of this local U6 checkpoint, its public budget contract, thin adapter and complete
saved evidence. Stop here. A second application-owned condition/effect context or
broader portability/SDK/runtime integration is only a candidate for a separately
selected bounded assignment; no integration or publication transport is started.

Confidence is high in executed Linux ownership/typed refusal/advisory compatibility,
medium in public spelling and chosen ceilings/work-unit longevity. These are agent
engineering choices, not personal user API preferences. Reversing private adapter/
format details is local; changing a public report/budget needs consumer/test review.
Allocator overhead/rounding/exceptions and input/compiler/read/format costs stay
outside charged payload/work; no latency guarantee is claimed. U6 does not inject
allocation failure. LSan, other platforms/C libraries, installed SDK/public ABI
portability and wider runtime integration remain unverified. Source comments are
reviewed against source/tests; no documentation generation, SDK export, broad suite,
GUI/device, package download, install, pin change, subagent or external agent ran.

## Retained U5 headless authored-dialogue inspection

STORYTELLING-U5 started from clean accepted U4
`509bfcb12157815f621dae8d1cd07c3622b43694`. Private report/helper, CLI selection,
focused graph tests and actual child-process cases are implemented. Configure and
incremental builds exited 0; discovery and execution contain exactly **98** cases,
including **15 new and all 83 retained names**. **98/98 passed**, with no skips,
unrun cases or ASan/UBSan diagnostics. All 54 real child-console outcomes match
expected statuses. Assigned native checks are complete, ready for coordinator
review. No Engine/public header, loader/schema/key allowlist, save/session contract
or pin changed.

## U5 caller and implemented contract

`dialogue-console --inspect-definition PATH` loads/compiles with the unchanged
private loader, calls `inspect_definition(*definition)` and prints
`format_definition_report(report)`, returning before Encounter/Session or stdin.
The header-first `dialogue_console_report.consumer` and literal report in
agent-notes.md show the complete small caller. Both decision views add matching
ST-013/014. Reports own definition/revision/entry/counts and three authored-order
ID lists after all input/draft/definition owners disappear. Terminal means the
compiled Terminal kind; nonfirst entry uses `Definition::entry()`. All choice
edges, including conditional/parallel/cyclic edges, participate without handlers.
Forward traversal starts at entry; reverse traversal starts at every terminal.

Output is explicitly advisory topology, not evidence of gameplay reachability,
effect history, termination or guaranteed completion. Closed cycles and other
advisories remain successful inspections, with no repairs/warnings added to the
compiler. IDs escape control bytes/quotes/backslashes; classic-locale counts and
authored-order lists give deterministic lines. Usage errors exit 2, rejected
input exits 3, valid reports exit 0, unexpected exceptions keep exit 1. Inspection
cannot combine interactive --definition or --trace-effects flags; normal built-in
and explicit-definition interactive commands retain their existing behavior.

The loader's 64 KiB/+1 read, 256-node/2048-choice, decoded string/aggregate/depth
limits remain unchanged. The independent helper rechecks node/edge/copied-ID caps
before report copies, uses subtraction-checked totals and fixed flat adjacency/
iterative visited queues. About 41 KiB of index/visited element storage is fixed,
excluding sanitizer/ordinary frame overhead. Target lookup costs O(N E L), L<=128;
then construction/traversal costs O(N+E). Exact scratch, string-copy and work
bounds/assumptions are in ST-014. Engine compilation alone has no consumer caps;
therefore direct compiled fixtures exercise true 256-node/2048-edge capacity and
+1 rejection, while actual JSON remains bounded more strongly by 64 KiB. No JSON
budget enforcement is claimed for arbitrary direct helper inputs.

## Actual U5 checks

Existing GNU/Linux Debug profile, CMake policy minimum 3.5, Engine-only headless
app linkage, OFF options, pins and incremental outputs are retained. Every
configure/build/native command uses the shared wrapper and one job. The current
adaptive memory policy/monitor controls cap/worker admission; no swap and the
4 GiB host admission floor remain in force. No cap manipulation, queue
cancellation/relaunch or unwrapped native child was used. ASan/UBSan remain
instrumented with halt-on-error; invocation-only detect_leaks=0 is ST-008's known
tracing accommodation. Leak detection remains **unverified**, not passing.

Commands from clone root (each exited **0**; complete stdout/stderr and immediate
exits are saved):

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target dialogue-console tests-dialogue-console tests-engine --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^(tests-engine\.(dialogue_definition|dialogue_session)\.|tests-dialogue-console\.dialogue_console_)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(dialogue_definition|dialogue_session)\.|tests-dialogue-console\.dialogue_console_)'
```

| Clone-local log under `builds/storytelling-u1-debug/` | Actual result |
| --- | --- |
| `u5-configure.log`, `.exit` | 0; existing profile retained. |
| `u5-build-1.log`, `.exit` | 0; private helper/main/cases compiled and linked, discovery completed. |
| `u5-build-2.log`, `.exit` | 0; final additional all-terminal/full reverse-seed boundary assertions compiled; no implementation defect or flag change. |
| `u5-registration.log`, `.exit` | 0; 98 nonempty cases. |
| `u5-registration-audit.log` | Exact source/discovery order matches; all 83 U4 names retained, 15 new. |
| `u5-ctest-1.log`, `.exit` | 0; 98/98 passed, 14.33 seconds, no skips/unrun cases. Original queue wait completed without cancellation/relaunch. |
| `u5-ctest-details-1.log` | Complete preserved LastTest.log: all 98 RUN/OK/CTest pass records and 54 child arguments/stdin/stdout/stderr/immediate statuses. |
| `u5-results-audit.log` | Discovery/execution names match; 32 retained plus 22 new child outcomes match expectations (total exits 0:21 / 2:15 / 3:18); no ASan/UBSan diagnostics. |

These logs stay ignored and clone-local. Children are spawned by the wrapped
native test process, inherit its sanitizer/resource environment and never acquire
the build lock themselves. Source fixtures are read-only; disposable inputs,
requested-save markers and transcripts belong to clone-local CTest workspaces.
Generated rules confirm ASan/UBSan on every changed translation unit and explicit
private pinned JSON includes remain in place. Five changed C++ files pass
clang-format dry-run/Werror; whitespace checks pass. No source defect required
an implementation fix or sanitizer/build-flag change.

| New passed cases | Observed boundary |
| --- | --- |
| report.consumer, crossroads | Header-first loader/report caller and literal complete gate/crossroads output, authored IDs/counts/order. |
| report.branch_rejoin, closed_cycle, terminal_and_loop | Nonfirst entry, branch/rejoin/cycle with terminal route, isolated unreachable terminal, closed cycle, terminal-only and zero-terminal self-loop; advisories never reject valid compiled structure. |
| report.ignores_conditions | Conditional/effect-bearing edges analyzed without a Session, flag store or handlers; explicit advisory distinguishes static edges from gameplay. |
| report.ownership | Input/draft mutation/destruction, weak definition owner actually expires, report/copy strings and output survive independently. |
| report.graph_capacity, id_capacity, display_escaping | Direct 256-node/2048-edge graph with parallel edges/long paths, +1 rejection, full 256-terminal reverse seeding, nonfirst terminal entry, 128-byte ID/+1, quoted/control/NUL/DEL output on one line per field. |
| transcript.inspect_gate, inspect_crossroads, inspect_advisories | Three actual inspection processes print exact literal reports, including a nonfirst-entry closed loop with condition/effect keys; stdin choice/effect/save/resume commands produce no encounter/effect/command output, save or source sidecar. Valid advisory exits 0. |
| transcript.inspect_rejections | Nine actual rejected inputs (truncation, escaped duplicate, multiple semantic errors, raw-NUL variants, trailing document, byte cap, missing file) exit 3 with exact existing origin/field/compiler formatting. Source bytes and source-directory entry counts are unchanged. |
| transcript.inspect_usage | Nine invalid argument combinations exit 2 before input; empty path exits 3. Missing/unknown/extra/repeated/mixed-mode arguments are deterministic. |

All 83 retained cases ran, preserving both interactive startup modes, both gate
routes, paired save/resume in same/fresh processes, live-incarnation rejection,
strict codec/file/definition rejection, immutable ownership and session failures.
The previous 32 child names/statuses are retained; U5 adds 22 (three valid exit 0,
nine usage exit 2, ten rejected input exit 3), for 54 total. Full output assertions
and source control flow establish inspection's no-handler/no-Session boundary;
absence of an effects trace alone is not claimed as its proof.

## U5 next action and limits

No assigned blocker remains. The shared native queue completed normally; no
resource refusal/exit 75 occurred. Next concrete action: coordinator review of
this coherent U5 source/records/checkpoint and the complete logs listed above.
Stop after U5. A second application-owned condition/effect context is only a
candidate for a separately selected unit; no implementation is started and no
user response is expected.

Confidence is high in semantics/ownership/bounds, medium in private CLI/report
spelling, with low local reversal cost. Cap changes require loader/storage/work
review. These are engineering choices, not evidence of personal API taste.
Portability/C libraries, LSan, SDK/runtime integration and read latency remain
unverified/outside scope; topology supplies no history/authentication guarantee.
No second application context/operation, graphical work, SDK/tutorial work,
sibling integration, publication transport or later unit starts in U5.

## Retained U4 bounded authored definitions

STORYTELLING-U4 started from clean accepted U3
`1a71629f51d0515d6203dc0dd014e1329da8f038`. The private loader, owned diagnostic
boundary, gate/crossroads JSON fixtures and explicit startup path are implemented;
**83/83 focused native cases passed**: 23 new plus all 60 accepted regressions,
with no skips/unrun cases or ASan/UBSan diagnostics. All 32 child consoles returned
their expected status: 18 normal exits 0, six usage exits 2, eight rejected-input
exits 3. Engine/public headers, Runtime, the accepted save codec and fresh-save
implementation remain unchanged. Both decision views add ST-011/012 and retain
ST-001/007's ownership/application policy. This accepts the assigned native U4
scope only; portability and wider integration remain unverified.

## U4 implemented contract

`dialogue-console --definition /chosen/gate.dialogue.json` reads and compiles the
complete explicit file before constructing Encounter/Session or processing input.
Omission preserves the built-in gate; a bad supplied path never falls back.
`load_definition(path)` returns a retained immutable definition or owned origin/
field/row context and every original compiler diagnostic. The header-first
`dialogue_console_definition.consumer` loads the authored gate, introduces,
captures/encodes/restores the idle pair, views and passes with one total effect.
Different crossroads IDs/text/revision use the same caller. Loading, decoding,
compilation and restore invoke no effects.

Exact schema/bounds are in ST-011/012 in both views: separate definition_schema 1;
root id/revision/entry/nodes; nodes id/text/kind/choices; choices id/text/target plus
optional string condition/effect. Supported names stay exactly condition introduced
and effect introduce, supplied by the private application, never data. Limits:
65536 document bytes plus one read detection byte, 256 nodes, 2048 total choices,
128 decoded bytes per ID/key/property, 4096 per text, 32768 aggregate node/choice
text, five container frames. No coercion, missing-field inference, arbitrary DOM,
unknown fields, duplicate decoded names, raw NUL or trailing data are accepted.

The byte cap is stronger than the 2048-choice guard: shortest complete rows alone
need 65535 bytes before node/document overhead. Actual input at that count rejects
as oversized; a valid production 2048-choice boundary is **not** claimed executed.
The lower-only private seam exercises the same global guard at three and four
choices across two nodes, retaining all other production limits. Exact 256-node
and +1, decoded string/text/aggregate and document-byte boundaries ran through real
files. These are reversible engineering choices, not personal API taste evidence.

## Actual U4 checks

The existing incremental GNU/Linux Debug configuration, policy minimum 3.5, OFF
selections, pins and outputs were preserved. All configure/build/native commands
used the shared-lock/resource wrapper, one job, ASan/UBSan and both halt-on-error
settings. No resource refusal/exit 75 or cancelled/relaunched queue wait occurred.
Command-local leak disabling remains ST-008; leak checking is **unverified**.
Actual commands below all exited **0**, from clone root:

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target dialogue-console tests-dialogue-console tests-engine --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^(tests-engine\.(dialogue_definition|dialogue_session)\.|tests-dialogue-console\.dialogue_console_)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(dialogue_definition|dialogue_session)\.|tests-dialogue-console\.dialogue_console_)'
```

| Complete clone-local log under `builds/storytelling-u1-debug/` | Actual result |
| --- | --- |
| `u4-configure.log`, `.exit` | 0; retained Debug/OFF selections and pins; only private app registration changes. |
| `u4-build-1.log`, `.exit` | 0; focused compilation/link/discovery. No source defect or sanitizer adjustment was needed. |
| `u4-registration.log`, `.exit` | 0; exactly 83 nonempty cases. |
| `u4-registration-audit.log` | Source/discovery names match in order, all 60 U3 cases retained, 23 new. |
| `u4-ctest-1.log`, `.exit` | 0; `100% tests passed out of 83`, 12.15 seconds, no skipped/unrun cases. |
| `u4-ctest-details-1.log` | Complete preserved LastTest.log; 83 matching RUN/OK/CTest pass records, all 32 child arguments/stdin/stdout/stderr/immediate statuses. |
| `u4-results-audit.log` | Executed/registered names match; expected child exits 0:18 / 2:6 / 3:8; no skips or ASan/UBSan diagnostics. |

Logs/status files stay ignored and clone-local, outside the commit. Generated rules
confirm explicit private pinned JSON includes for new translation units, unchanged
ASan/UBSan instrumentation and Engine-only app linkage. Children inherit the wrapped
test's resource/sanitizer envelope and never reacquire the lock. Disposable file/
transcript fixtures stay under clone-local CTest workspaces; authored source
fixtures are read-only at runtime. All seven changed/new C++ files passed repository
clang-format dry-run and whitespace checks.

| New passed cases (dialogue_console prefixes) | Observable boundary |
| --- | --- |
| definition.consumer, routes, second_definition | Header-first caller, loaded gate order/text and both routes, unavailable pass, one-effect paired resume; different crossroads data, initial/terminal snapshots, zero effects. |
| definition.ownership, owned_diagnostics | Stable retained definition/session/views after bytes and drafts are mutated/destroyed; complete owned compiler vector and structural origin/field/node/choice context after source destruction. |
| definition.semantic_errors, order_cycles_scope | Entry/target/empty/scoped duplicate IDs, shape and unsupported/cross-domain keys, including unreachable errors; exact ordered engine diagnostics retained. Nonfirst entry, cross-node choice reuse and explicit cycles remain valid. |
| definition.duplicates, unknown_missing, shapes_types, schema_revision | Every object's duplicate/unknown/missing fields, escaped-equivalent names, shapes/types/numeric booleans, invalid/unsupported schemas, revision integer/range/overflow. Positive uint64-max revision is valid; mismatched save rejects with live state/tokens preserved. |
| definition.limits_strings, limits_text, limits_nodes_choices, malformed_depth, raw_nul_bytes, io_and_formats | Exact caps/+1, decoded UTF-8 bytes, global choices via lower guard, aggregate text, bounded depth, malformed/truncated/trailing/second documents, all raw-NUL suffix variants, invalid paths and distinct authored/save formats. Every rejected file preserves exact bytes and creates no sidecar. |
| transcript.authored_gate_routes, authored_gate_resume, authored_second | Real --definition runs for gate and crossroads; same-field resume invalidates old presentation. Same-owner restore/pass reports one effect; fresh loaded and built-in processes restoring the save report zero. |
| transcript.authored_mismatch, authored_invalid_startup, authored_usage | Different valid ID/revision definitions reject incompatible saves without flag/presentation changes. Invalid startup exits 3 and usage exits 2, with no encounter/effect output, command processing, fallback or output next to source files. |

## U4 limits and next action

No assigned execution blocker remains. Other platforms/C libraries, leak detection,
installed SDK use and wider runtime integration are unverified/outside this private
startup unit. Capped bytes/work do not promise external file-read latency. Exact
revision checks supply no content authentication/history proof; same-ID/revision
edits remain application discipline. Flags belong to the serialized owner, never
topology. Existing fresh-save/no-replacement/no-durability and session/incarnation/
fault contracts remain in force. No broad campaign, GUI/device, dependency fetch,
install, pin change, SDK export, documentation build, subagent or external agent
was started.

Confidence is high in executed staging, ownership, rejection and operation
boundaries; medium in private format spelling/text caps. Caps/CLI changes have low
local reversal cost; changed schema meaning needs explicit versioning. The stronger
byte cap and lower guard check remain distinguished above. Next concrete action:
coordinator source/log review of this local U4 checkpoint and selection of later
bounded work. Stop here; no next implementation, sibling integration, recurring
scheduling or publication transport is started.

## Retained U3 console / paired JSON acceptance

STORYTELLING-U3 is implemented and accepted within the assigned native scope, from
clean U2 `639fbf505cee4f0970efb7fa4f2ce2253c97abc4`. **60/60 focused cases passed**:
26 new consumer cases plus all 34 definition/session regressions, with no skipped
or unrun cases. Ten actual console child processes each exited 0. No Engine API,
U1/U2 source, dependency pin or sanitizer flag changed. The default-OFF
CHERYL_BUILD_DIALOGUE_CONSOLE option and directly necessary app/test registration
are the only build configuration edits. The app links Engine without optional
modules and includes pinned JSON 3.11.3 privately, as do its JSON test cases.

The header-first `dialogue_console_state.consumer` caller introduces, captures the
gate pair, encodes/decodes/resumes, rejects its old incarnation, views and passes
with the effect count still one. Both decision views describe ST-001/007's concrete
consumer/paired owner, ST-009's fixed-envelope SAX decoder and ST-010's exclusive
fresh-path policy. Proposed U3 behavior is now implemented and executed in this
configuration. These are engineering choices, not new personal preference evidence.

## Actual U3 checks

The existing incremental GNU/Linux Debug build, CMake policy minimum 3.5 and all
previous OFF module/test-campaign selections were retained. Configure/build/native
checks used the existing shared-lock/resource wrapper, one job, ASan/UBSan and
both halt-on-error settings. Normal queued commands were retained and completed
in their original sessions. No exit 75 or other admission refusal occurred.
Command-local leak disabling remains ST-008's tracing accommodation; leak checking
is **unverified**, not passing. No broad campaign, GUI/device, SDK export,
documentation generation, installation, dependency fetch or external agent ran.

Actual commands from clone root (each exited **0**, full stdout/stderr captured):

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug \
  -DCHERYL_BUILD_DIALOGUE_CONSOLE=ON
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target dialogue-console tests-dialogue-console tests-engine --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^(tests-engine\.(dialogue_definition|dialogue_session)\.|tests-dialogue-console\.dialogue_console_)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(dialogue_definition|dialogue_session)\.|tests-dialogue-console\.dialogue_console_)'
```

| Clone-local log under `builds/storytelling-u1-debug/` | Actual result |
| --- | --- |
| `u3-configure.log`, `.exit` | 0; narrowly enabled console, preserved Debug/OFF selections and pins. |
| `u3-build-1.log`, `.exit` | 0; app/private state library/new runner compiled, linked and discovered; Engine regressions retained. No implementation fix was required. |
| `u3-registration.log`, `.exit` | 0; exactly 60 cases, including 26 new and all 34 accepted regression names. |
| `u3-registration-audit.log` | Text audit matches all 60 source TEST declarations to discovery in order, no missing/extra names. |
| `u3-ctest-1.log`, `.exit` | 0; `100% tests passed out of 60`, 8.42 seconds; --no-tests=error and parallel 1. |
| `u3-ctest-details.log` | Complete preserved LastTest.log, all 60 RUN/OK/Test Passed records, plus complete stdin/stdout/stderr and immediate status for all ten child-console transcripts. |
| `u3-results-audit.log` | Registration/executed names match; 60 RUN/OK/passes, ten child exits 0; no skips or ASan/UBSan diagnostics. |

Logs stay ignored and clone-local, outside the source/records commit. Immediate
command statuses were saved before later inspection. Generated Ninja rules were
read to confirm explicit private JSON includes for state/case targets, ASan/UBSan
instrumentation and Engine-only app linkage. No child reacquires the build lock;
POSIX spawn inherits the wrapped test's environment/resource envelope. Transcript
fixtures use disposable directories inside the clone-local CTest workspaces.

| New suite and passed names | Executed observable boundary |
| --- | --- |
| `dialogue_console_state`: consumer, silent_route, paired_boundaries, adoption_authority (4) | Both outcomes, authored order/text, unavailable/stale rejection, paired initial/gate/terminal values, retained definition, nonthrowing staged adoption and old-token renewal. |
| `dialogue_console_codec`: ownership_roundtrip, malformed_trailing, duplicate_fields, unknown_missing, shapes_types, schemas_ranges, content, raw_nul, limits_nesting (9) | Owned decoded values; every object level's duplicate/unknown/missing fields; escaped-equivalent keys; wrong shapes/types/numeric booleans; integer overflow and unsupported/noninteger schemas; empty/mismatched IDs/revision/node; truncated/malformed/trailing/second documents; all three specified NUL suffix variants and interior NUL; exact 64 KiB and oversize detection; deep input rejected without a recursive DOM. |
| `dialogue_console_files`: fresh_roundtrip, existing_files_links, invalid_missing_paths, deterministic_failures, owned_cleanup_failure, cleanup_only_after_failure (6) | Real exclusive save/load; exact existing file bytes and leaf/dangling-link referents preserved; empty/NUL/missing paths; deterministic open, real partial write, flush, close and cleanup failures; incomplete owned output removed or explicitly reported; unowned files never cleaned up. |
| `dialogue_console_transcript`: admitted, departed, same_owner_resume, fresh_process_resume, failed_io_preserves_offer, initial_terminal_saves, quit_eof_bad_syntax (7) | Actual executable routes, visible authored order/text, unavailable/unknown/stale commands, same-field resume, explicit save/resume, no false success after I/O rejection, preserved presented choices after failed load, initial/terminal saves and clean quit/EOF. |

Each codec rejection case crosses the real file/decode/resume boundary from BOTH
initial false-flag and introduced true-flag gate states. It compares the entire
active pair and exact source bytes, checks unchanged effect count, then uses the
old issued token successfully. Successful same-field resume rejects old tokens
as WrongIncarnation in helper tests; numbered console presentations reject old
commands too. Maximum uint64 step decodes/resumes but rejects choosing before
effects. Edited valid step/flag values are adopted literally, without inferred
history. The same-owner transcript reports exactly one introduce effect after
save/resume/pass; the fresh-process transcript reports zero. Effect counts are
opt-in process-local test instrumentation, absent from saved data.

## U3 limits and next action

No assigned U3 execution blocker remains. Acceptance is limited to this GNU/Linux
Debug Engine-linked configuration; other C libraries/platforms and leak detection
remain unverified. Exclusive C11 fopen x creation plus checked write/flush/close is
the complete file policy: no overwrite, rename publication, fsync/power-loss
durability, hostile directory-race protection, multiwriter protocol or durable
external-effect guarantee. Cleanup failure reports the incomplete owned path and
requires deliberate caller recovery; the console never claims that save succeeded.
One serialized owner, idle adoption and coherent boolean/snapshot capture remain
application obligations. The schema is unauthenticated and intentionally has no
migration or gameplay-history proof. Engine revision discipline and API taste
uncertainties from U1/U2 remain unchanged.

Confidence is high in the executed ownership/rejection/file boundaries, medium
in command spelling and private format longevity. Command/adapter changes have
low local reversal cost; changed schema semantics need explicit versioning.
U3's next action was coordinator source/log review; the coordinator accepted it
and assigned U4 above. Its publication transport remains with the coordinator.

## Retained U2 acceptance evidence

U2 implements the CPU session/value unit on top of preserved checkpoint
`f12adbbf9a3edda5c6ad83115deabc502d296f4b`: retained frozen definition, owned views,
opaque live incarnation, checked explicit choices, synchronous borrowed handlers,
failure state and owned `Snapshot` resume. **18/18 session cases and all 16
definition regressions passed** (34/34, no skips/unrun cases). A standalone
allocation-rejection probe compiled and ran successfully. This accepts the assigned
U2 scope only. Console/JSON and filesystem I/O were left for U3; runtime integration
remains outside the first milestone.

Both contract views are current: [agent-notes.md](agent-notes.md) and
[decisions.md](decisions.md), especially ST-002/003/004/005/007. The private shared
incarnation renews on every start/resume, survives idle moves and is absent from
snapshots; equal persisted fields cannot resurrect an old token. The tiny gate
caller captures/adopts the flag and cursor on one idle serialized owner, resumes,
then chooses pass with the effect counter still one. This is an engineering
implementation of the coordinator's correction, not evidence of personal API taste.

## Actual U2 checks

All configure/build/native checks used the existing wrapper, one job and
clone-local outputs. The existing Debug configuration/pins were retained. The
incremental build automatically reconfigured for the new source/test globs;
no repository build configuration was edited. ASan/UBSan instrumentation and both
halt-on-error settings remain active, with command-local leak disabling under
ST-008. No resource refusal/exit 75 occurred. Normal shared-lock queueing was
allowed to complete in the existing tool session without cancellation/probes.

| Session case(s), all passed | Observable boundary |
| --- | --- |
| `consumer`, `routes`, `boundaries` | Header-first tiny caller; one flag; both endings; initial/terminal snapshots; paired resume without repeating the effect. |
| `conditions`, `scope`, `missing_handlers` | Recheck availability in both directions after presentation; foreign/stale/unissued/unknown selections and missing handlers reject without effects/progress. |
| `cycles`, `incarnations`, `independent` | Explicit cycle progression invalidates old steps; identical-field resume/rewind and repeated logical IDs get new markers; independent cursors reject foreign commands. |
| `ownership`, `moves`, `handlers` | Retained definition lifetime; view/snapshot ownership after definition/session release; marker survives idle moves; moved-from state rejects; no handler retention. |
| `guard_failure`, `effect_failure`, `reentrant` | Predicate failure preserves healthy progress/no effects; original effect exception rethrows after partial app mutation and faults; callback view/choose/snapshot return Busy. |
| `lifecycle`, `exhaustion`, `resume_rejects` | Active move/replacement/destruction terminate in death tests; maximum step rejects before callbacks; schema/content/revision/session/node failures preserve an existing live value. |

The focused allocation probe is isolated from `tests-engine`; only its own
executable replaces scalar new/delete. It first proves that a one-byte request
is rejected, then simulates owned view/snapshot copy failure with 128-byte strings
and verifies unchanged healthy state. Its effect verifies the key borrows frozen
storage, then enables allocation rejection until `choose` returns. Success,
unchanged rejection count, one effect and destination/step checks passed. The
engine's prepared `expected<void, SessionError>` success is statically nonthrowing
to copy; the source after effect return contains only numeric assignments, return
and the nonthrowing busy-guard reset. No memory exhaustion/OOM was attempted.

Actual U2 commands (from clone root), all exit **0**; complete stdout/stderr and
immediate exit statuses are saved under `builds/storytelling-u1-debug/`:

```sh
/mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -Wall -Wextra -Wpedantic -fsyntax-only \
  -Iprojects/engine/include -Iprojects/engine/include/cheryl -Iextern/googletest/googletest/include \
  projects/engine/src/core/game-framework/dialogue-session.cpp projects/engine/tests/unit/src/dialogue-session.cpp
# Above checked the initial tiny caller before the other 17 session cases were added.

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^tests-engine\.(dialogue_definition|dialogue_session)\.'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 -R '^tests-engine\.(dialogue_definition|dialogue_session)\.'

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -O0 -g -Wall -Wextra -Wpedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Iprojects/engine/include -Iprojects/engine/include/cheryl \
  projects/engine/src/core/game-framework/dialogue.cpp projects/engine/src/core/game-framework/dialogue-session.cpp \
  projects/engine/tests/probes/dialogue-session-allocation.cpp -o builds/storytelling-u1-debug/dialogue-session-allocation-probe
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- builds/storytelling-u1-debug/dialogue-session-allocation-probe
```

| Complete clone-local log / matching `.exit` | Actual result |
| --- | --- |
| `u2-caller-syntax.log` | Exit 0, initial small caller and implementation syntax/type check, no diagnostics. |
| `u2-build-1.log` | Exit 0, automatic glob reconfiguration, Engine/source/test compilation, linking and discovery. |
| `u2-registration.log` | Exit 0, exactly 34 cases: 16 definition and 18 session names, matched to source in order. |
| `u2-ctest-1.log` | Exit 0, `100% tests passed out of 34`, 4.64 seconds; no skips/unrun cases. |
| `u2-ctest-details.log` | Preserved complete `Testing/Temporary/LastTest.log`; all 34 GoogleTest RUN/OK records and CTest passes checked against source. |
| `u2-allocation-build.log` | Exit 0, isolated warning-enabled Debug ASan/UBSan source probe compilation. |
| `u2-allocation-run.log` | Exit 0, allocation rejection and post-effect commit checks passed, no sanitizer diagnostics. |

The four changed/new C++ files passed repository clang-format and whitespace
checks. The 16 definition tests/source remain unchanged; only the public header
adds session declarations and the standard functional include. No broad suite,
SDK export, GUI or documentation generation ran. Full logs remain clone-local
ignored outputs, outside the coherent source/records commit.

## U1/U1V evidence retained

U1 source is committed in `25836ffb48fb25e9ed5e89d2805daf6e093ed5fc`, from design
checkpoint `716ec60984e6764a03ed375ac37826e5d92e6978`. U1V started at clean checkpoint
`85b2280c512cd6638c1e67819a9539d2a44216e5` and completed definition acceptance:
**16/16 focused native cases passed**, with no skipped or unrun cases. Debug
ASan/UBSan instrumentation and halt-on-error remained active. Leak detection is
unavailable under the reproduced LSan tracing limitation (ST-008), not passing.
The font prerequisite remains resolved: the coordinator
initialized assets at the unchanged pin
`98453b2a6e0edfcfba22eea3fde801526d72b861`, and
`assets/fonts/DejaVuSans.ttf` is present. The prescribed configuration now succeeds.
The incremental `tests-engine` target and its discovery now succeed with the
authorized invocation-local environment adjustment. No repository build
configuration, dependency pin, global setting or U1 source changed during U1V.

Contracts: [agent-notes.md](agent-notes.md); compact choices:
[decisions.md](decisions.md), ST-001/002/003/004/006 and verification decision ST-008.
ST-005/007 were proposals at U1V; U2 now implements the value boundary as recorded
above. The U1 header/source implement owned
authored values, private deep-copy freezing, ordered inspection, scoped lookup and
owned diagnostics.
At U1/U1V the four-node gate was definition test data only; the session and private
console were added in subsequent units recorded above.

## Actual checks and registration

The new test translation unit includes the public header first. It uses the
existing unit `src/*.cpp` glob: `projects/engine/tests/unit/CMakeLists.txt` feeds
`tests-engine-cases` through `cheryl_add_test_suite`, and
`cmake/CherylTests.cmake` calls GoogleTest discovery with prefix `tests-engine.`.
There are 16 `TEST(dialogue_definition, ...)` declarations in the source. The
generated `cmake_test_discovery_fc8c995a1d.json` and
`tests_engine_6c1488ca_tests.cmake` under the unit build directory contain those
16 cases. The actual filtered CTest listing selected exactly those names in source
order, and the subsequent run executed all 16. Per-case GoogleTest `[ RUN ]` /
`[ OK ]` output and CTest `Test Passed.` records were checked against the source.

| Passed case(s) | Observable boundary exercised by the native selection |
| --- | --- |
| `consumer`, `ownership`, `lookup` | Header-first caller; constructor/copy/move restrictions; draft aliases remain separate even through std::move; draft mutation/destruction and retained-handle lifetime; stable text/order/keys; exact lookup and misses. |
| `empty_ids`, `revision`, `duplicate_nodes`, `duplicate_choices`, `missing_references` | Empty definition/entry/node/choice/target, zero revision, scoped duplicates, unknown entry/target and empty node collection reject without a partial definition. |
| `unsupported_keys`, `supported_names` | Unsupported or empty optional keys and empty/duplicate declared names reject in the correct condition/effect domain, with owned index/field/value diagnostics. |
| `node_shape` | Prompt without choices, terminal with choices and unknown kind reject. |
| `independent_domains`, `cycles`, `entry_order` | Reused choice name across nodes, equal operation spelling across domains, explicit cycles, empty text, independent same-definition compilations, and nonfirst/terminal entry preserve their intended semantics. |
| `unreachable`, `diagnostics` | Invalid unreachable row rejects eagerly; multiple diagnostics have deterministic authored order, own location/offending values after input destruction, and expose no partial frozen output. |

Validation has no application-code parameter or stored callable; source review
confirms it only processes these concrete CPU values. A single `Draft` contains
one definition ID, so duplicate-definition checking across compilations is not
representable here and is deliberately not replaced with a global registry.

## Configuration retained

U1V reused the existing clone-local GNU C/C++ 16.2.1 Debug configuration and
incremental objects. The following configure already exited **0** after the
pinned font was restored; it was not rerun during U1V:

```sh
/mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCHERYL_BUILD_TESTS=ON \
  -DCHERYL_BUILD_NATIVE_GLFW=OFF -DCHERYL_BUILD_NATIVE_SDL3=OFF -DCHERYL_BUILD_OPENGL=OFF \
  -DCHERYL_BUILD_STARTUP=OFF -DCHERYL_BUILD_WORKERS=OFF -DCHERYL_BUILD_WORKER_TOPOLOGY_HWLOC=OFF \
  -DCHERYL_BUILD_WORKER_TOPOLOGY_JSON=OFF -DCHERYL_BUILD_DEBUG_TERMINAL_LINUX=OFF \
  -DCHERYL_BUILD_UI_TGUI=OFF -DCHERYL_BUILD_UI_RMLUI=OFF -DCHERYL_BUILD_AUDIO_MINIAUDIO=OFF \
  -DCHERYL_BUILD_DOCS=OFF -DCHERYL_BUILD_SDK=OFF -DCHERYL_BUILD_DEMO=OFF -DCHERYL_BUILD_FIRST_GAME=OFF \
  -DCHERYL_BUILD_TUTORIAL_GAME=OFF -DCHERYL_BUILD_ALL_TESTS=OFF -DCHERYL_BUILD_ACCEPTANCE_TESTS=OFF \
  -DCHERYL_BUILD_CONSUMER_TESTS=OFF -DCHERYL_STARTUP_OPENGL=OFF
```

The cache retains all assigned OFF settings, tests ON, Debug and policy minimum
3.5. Both dialogue object rules retain `-fsanitize=address,undefined`. The public
header still exposes standard CPU values and Engine visibility macros only, with
no JSON, UI or runtime dependency. The consumer includes that public header first.

## Discovery diagnosis and actual U1V commands

All commands below ran from this clone through the existing shared-lock wrapper,
with one compilation/test job and its fixed resource controls. A queued live
command is normal serialization: retain its tool session and wait with updates.
The previous checkpoint's cancelled queue waits are historical, not an unresolved
admission blocker. No admission exit **75** occurred during U1V. If a later command
returns **75**, record its exact refusal and checkpoint without an unchanged retry.

Discovery with leak detection still at its default setting and halt-on-error
explicit exited **1**:

```sh
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -P \
  builds/storytelling-u1-debug/projects/engine/tests/unit/tests_engine_6c1488ca_discovery.cmake \
  > builds/storytelling-u1-debug/u1v-discovery-default.log 2>&1
```

Complete output captured this clone's causal error:

```text
==5==LeakSanitizer has encountered a fatal error.
==5==HINT: For debugging, try setting environment variable LSAN_OPTIONS=verbosity=1:log_threads=1
==5==HINT: LeakSanitizer does not work under ptrace (strace, gdb, etc)
```

CMake reported `Error running test executable.` and `Result: 1`; this was not its
five-second timeout. The coordinator-authorized ST-008 accommodation below is
invocation-only. It disables only leak detection, retains ASan/UBSan and both
halt-on-error settings, and changes no timeout or build flag. Each command exited
**0**; the first completed incremental linking and discovery, the second listed
exactly 16 cases, and the third ran all 16:

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine --parallel 1 > builds/storytelling-u1-debug/u1v-build.log 2>&1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^tests-engine\.dialogue_definition\.' > builds/storytelling-u1-debug/u1v-registration.log 2>&1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 -R '^tests-engine\.dialogue_definition\.' \
  > builds/storytelling-u1-debug/u1v-ctest.log 2>&1
```

Complete clone-local logs live under `builds/storytelling-u1-debug/` and remain
outside the commit. Each command's status was saved immediately to the matching
`.exit` file; test details were copied before a later run could overwrite them.

| Log | Actual result |
| --- | --- |
| `u1v-discovery-default.log`, `.exit` | Exit 1, reproduced LSan tracing failure with complete CMake diagnostic/listing. |
| `u1v-build.log`, `.exit` | Exit 0, incremental `tests-engine` link and generated discovery completed. |
| `u1v-registration.log`, `.exit` | Exit 0, `Total Tests: 16`, exactly the authored names in source order. |
| `u1v-ctest.log`, `.exit` | Exit 0, `100% tests passed out of 16`, real time 1.81 seconds; no skips or unrun cases. |
| `u1v-ctest-details.log` | Complete copy of `Testing/Temporary/LastTest.log`; all 16 GoogleTest cases ran and passed, matching source and registration. |

The earlier syntax/header check at source checkpoint `25836ff` also exited **0**,
after final C++ edits/formatting. It is supporting compilation evidence, separate
from U1V's actual runtime checks:

```sh
/mnt/projects/cheryl-features/scripts/build -- c++ -std=c++23 -Wall -Wextra -Wpedantic -fsyntax-only \
  -Iprojects/engine/include -Iprojects/engine/include/cheryl -Iextern/googletest/googletest/include \
  projects/engine/src/core/game-framework/dialogue.cpp projects/engine/tests/unit/src/dialogue-definition.cpp
```

The three new C++ files passed `clang-format --dry-run --Werror` at that checkpoint
using the repository style. U1V required no source fix or API change. Record links,
matching IDs, decision lengths, commands/results and staged whitespace are checked
for the follow-up records. No broader suite, SDK export, GUI or documentation
generation ran during U1V. Its four-node gate was definition test data only then.

## Limits and next coordinator selection

No U1 execution blocker remains within the assigned definition scope. Confidence
is high in the reproduced LSan diagnosis and the behavior exercised by the 16
native cases. Leak coverage remains unavailable here; remove only `detect_leaks=0`
on an LSan-compatible executor to restore it while retaining both halt-on-error
settings, with no code/configuration reversal cost. Acceptance covers this
GNU/Linux Debug Engine-linked runner only.
API spelling remains an agent engineering choice with medium confidence, not
established personal user taste. U2's synchronous dispatch and value snapshots
are implemented/executed as recorded above; the current U3 section owns
JSON/console acceptance status.

U2 has no unresolved execution blocker. Confidence is high in the marker and
failure/commit boundaries from source review, the 34 native passes and isolated
probe. The hard active-lifecycle termination policy is a medium-confidence
engineering choice: it keeps idle adoption noexcept and prevents callback storage
invalidation, but does not recover programming misuse. Changing it is cheap while
only these initial callers exist. One serialized owner, stable handler lifetimes,
pure predicates and coherent application flag pairing remain caller obligations.
Leak checking is unavailable here; no durable external-effect/rollback guarantee
or migrated snapshot compatibility is claimed.

U2 proposed U3, which was implemented and accepted before the current U4 assignment.
No unit beyond U4 is selected. Stop after U4's local checkpoint for coordinator
review and later bounded selection, without a question or automatic continuation.
