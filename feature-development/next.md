# U10 private offline creative encounter sets

STORYTELLING-U10 starts from the requested clean completed U9 HEAD
`4f5e7eab4af961d95f5a04d6f4f079517e39a758`, on feat/storytelling. Source, current
records, ready U9 receipt and complete 154-pass/54-child evidence agree with the
prerequisite. Workspace/clone instructions, autonomy/memory policy, storytelling
brief and story-generation.md were read. This explicitly assigned continuation
is outside the completed three-commit batch; earlier receipts stay unchanged.

Implementation and focused execution are complete: **176/176 passed**, **all 154
accepted U9 names/order retained**, with 18 new private model/owner cases and four
new executable transcript cases. There are zero skipped/unrun cases or ASan/UBSan
diagnostics. All **54 prior full child transcripts are byte-identical** to U9
(21 exit 0, 15 exit 2, 18 exit 3). Nine new actual child processes have expected
outcomes (4 exit 0, 4 exit 2, 1 exit 3); the default child plays eight finite
scenarios. Receipt [story/STORYTELLING-U10.json](story/STORYTELLING-U10.json) reports
worker execution, not automatic coordinator source acceptance or untested quality.
Final source review initialized the host method enum and added an invalid-method
refusal assertion; the targeted rebuild and **22/22 private recheck** also pass,
with all nine child outputs/statuses unchanged. The accumulated suite ran once.

## Implemented caller and creative boundary

```cpp
namespace S = OfflineStory; // Private story-offline/src/story.h, not an Engine API.
const auto content = S::accept(S::manual_sets()[0], S::ferry_catalogue(), {}).value();
const auto graph = S::project(content, 0).value();
auto encounter = S::Encounter::start(graph, {"ferry-01"}).value();
const auto offered = encounter.view().value();
encounter.choose(offered.choices[0].selection).value(); // Actual checked Session.
const auto saved_pair = encounter.capture().value();
encounter.restore(saved_pair).value();                // No repeated fare/reward.
const auto fresh = encounter.view().value();           // Old tokens lost authority.
encounter.choose(fresh.choices[0].selection).value();   // Explicit Continue.
```

The real `story-offline` executable prints Proposed/manual, Accepted, issued choices,
NPC prose, actual facts and each observer's knowledge/belief/attitude. No arguments
run both manual sets' three actions and alternate-pay flavours (eight scenarios).
CLI `story-offline tide|storm pay|repair|decline [0|1 [none|restore|drain]]` selects
one finite scenario. Usage rejects before play with exit 2; checked refusal exits
3, success 0. `drain` changes live resources after presentation to demonstrate
availability recheck. This is a bounded action selector, not a free-text parser.

Both pay and repair are available together from 8 coins/3 planks. Tide costs three
coins or two planks; storm costs five coins or one plank, with distinct repair
relationships. Routes rejoin destination after separate NPC replies; decline
ends ashore with supplies intact. Two flavours per action change wording/response
without changing mechanics. Typed host-approved plans, not prose or callbacks,
fix the costs/rewards/targets. Mara recognizes and approves; Ivo disapproves under
a mistaken belief despite factual licensed passage; Lena remains unaware. No
global reputation, arbitrary operations or observer belief can rewrite those facts.

ST-021/022/023/024 match both decision views. Private Accepted/Projection factories
deep-copy into actual const objects and retain their ownership. Count/text/work/
payload preflight precedes copies/projection; ordinary allocation exceptions
propagate without partial publication. Projected Draft goes through current
Engine compilation into at most six nodes/six edges. Public Engine APIs, indices,
Session authority, existing console JSON/schema/file rules and Runtime are unchanged.
The only root wiring is a default-OFF CHERYL_BUILD_STORY_OFFLINE option and its
private app/test directory. No JSON parser, provider/package/pin, sibling dependency,
magic integration, implicit callback routing or Engine source copy was added.

Availability is pure. The serialized owner stages/validates complete scalar state,
then adopts it nonthrowingly inside the checked effect; U9 prepares its owned
success before that effect and commits cursor/result without allocation afterward.
Pair restore validates state and constructs fresh Session before idle nonthrowing
adoption. Failed restore preserves exact old pair/live token; success renews
incarnation and never replays effects. Instrumentation counts are not saved ledgers.

## Actual native commands and evidence

Reused builds/storytelling-u1-debug and incremental outputs, preserving Debug/shared
Engine, all prior OFF choices, policy minimum and pins. Only the new private app
was enabled. All configure/build/discovery/execution ran through the shared wrapper,
one job, under existing adaptive/no-swap/admission limits. Original command sessions
were kept through completion. No resource refusal, native failure or cancellation
occurred. Review hardening justified one private rebuild/recheck below; unchanged
Engine/console regressions were not repeated. ASan/UBSan and halt-on-error remain enabled;
command-local detect_leaks=0 is ST-008's tracing accommodation, not passing leaks.

Executed from clone root, immediate exits **0**:

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug \
  -DCHERYL_BUILD_STORY_OFFLINE=ON
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target story-offline tests-story-offline tests-engine tests-dialogue-inspection dialogue-lookup-allocation dialogue-transition-allocation dialogue-console tests-dialogue-console --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection|dialogue_lookup|dialogue_route|dialogue_transition)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.|tests-dialogue-(lookup|transition)\.allocation$|tests-story-offline\.)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection|dialogue_lookup|dialogue_route|dialogue_transition)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.|tests-dialogue-(lookup|transition)\.allocation$|tests-story-offline\.)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target story-offline tests-story-offline --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 -R '^tests-story-offline\.'
```

| Saved files under builds/storytelling-u1-debug/ | Actual result |
| --- | --- |
| u10-configure.log, .exit | 0; prior Debug/OFF configuration/pins preserved, private app ON. |
| u10-build-1.log, .exit | 0; real shared Engine-linked app/model/focused runners compile, link and discover. |
| u10-registration.log, .exit; u10-registration-audit.log | 0; 176 unique nonempty names; all 154 U9 names/order plus exactly 22 new source names. |
| u10-ctest-1.log, .exit | 0; 176/176 passed in 20.96 seconds; accumulated suite ran once. |
| u10-ctest-details-1.log | Complete preserved LastTest.log, 174 matching GoogleTest RUN/OK pairs plus two probes; all 63 actual child arguments/output/errors/immediate exits. |
| u10-results-audit.log | Result/name agreement; 54 old child blocks byte-identical; nine new outcomes as expected; no skips/unrun cases or ASan/UBSan diagnostics. |
| u10-demo-transcript.log | Unchanged extraction of actual paid/paired-restore child and eight-scenario default child's complete output from details log; no extra execution. |
| u10-boundary-rules.log | Seven instrumented private compile rules, both actual Engine-linked executables; no export patch/private source copy/new platform/JSON dependency; prior OFF options unchanged. |
| u10-audit-diagnostics.log | Read-only link matcher initially exited 1: AssertionError ('tests-story-offline', 0). Corrected for CMake's second explicit Ninja output; static audit now passes. No native failure or rerun caused by this audit. |
| u10-build-2.log, .exit; u10-ctest-2.log, .exit | 0; final review initialized Outcome::method and added invalid-enum rejection coverage. Only private targets rebuilt, 22/22 private cases rechecked in 2.63 seconds. |
| u10-ctest-details-2.log; u10-final-audit.log | Full final private execution, all 22 names/order and nine full child blocks unchanged, no skips/unrun cases or ASan/UBSan diagnostics. |

| New passing cases | Meaningful boundary |
| --- | --- |
| caller, routes, flavours | Actual issued choices; two meaningful actions available; independent literal costs/resources/attitudes/endings for six routes; two flavours preserve keys/targets/state while changing prose. |
| ownership, pure_preparation, prose_not_authority | Mutate/destroy all candidate/catalogue/context sources, retain owner/frozen definition; validation/projection/compile/inspection/views have no effects; false reward prose cannot grant authority. |
| eligibility, observers | Changed live resources reject before mutation; surviving eligible token works and stale token rejects; knowledge/belief/evaluation cannot rewrite licensed facts. |
| paired_resume, failed_resume | Same/fresh owner restores invoke no reward; old incarnation rejects, seven malformed pairs preserve old token/state, wrong flavour rejects; edited step does not replay or prove rewards. |
| unknowns, duplicates, conflicts, owning_diagnostics | Unknown actor/op/condition/reward, broken refs, scoped duplicates, conflicting costs/flags/actors/methods/context reject before play; diagnostics survive source destruction. |
| count_limits, text_limits, storage_work, invalid_limits | Exact/one-short/zero count/text/work/payload budgets; 1024/1025-byte strings, 64/65-byte IDs, control bytes, above-ceiling/size_t-maximum requests; old accepted/projected values survive refusal. |
| Four story_offline_transcript cases | Nine real inherited-envelope children: paid paired restore, storm repair/alternate flavour, decline, live denial, four usage failures, finite eight-scenario demonstration with literal expected output/state. |

Logs/statuses are ignored, clone-local, nonempty regular files. Children inherit the
wrapped sanitizer/resource envelope without reacquiring the native lock. Static
format/whitespace and matching decision/receipt/log-path checks accompany staged
review. No broad suite, GUI/device, SDK/docs build, provider/download/install,
subagent/external agent, protected/sibling cache or publication was used.

## Limits, confidence and exact next action

No U10 execution blocker remains. Confidence is high in the exercised private
ownership/authority/paired-restore contracts; medium in vocabulary, caps, observer
policy and pacing. The lower-only ceilings and exact accounting exclusions are
in ST-024: storage is requested private payload, not Engine compile internals,
allocator rounding/control blocks or RSS; work is declared units, not instructions/
time. Compiler inputs remain structurally capped. Unicode/prose correctness is not
validated. Fixtures are manually authored, **not actual model output**. No provider
quality, human fun/playtest, graphical behavior, other platform/Release, SDK/runtime
integration or leak/durability coverage is established. An edited snapshot/state
is not authenticated history; no production transaction, disk format, rollback of
external effects or exactly-once crash guarantee is claimed.

Next action: **coordinator reviews this private U10 implementation, receipt and
saved evidence, then selects any further bounded creative/playtest work.** Worker
is idle after this checkpoint; no provider, free-text parser, public story service,
global reputation, magic integration or subsequent unit starts automatically.

---

Earlier milestone records below remain historical evidence; U10 above owns the
current result and next action.

# U9 owned successful transition observations

STORYTELLING-U9 starts from clean completed U8
`66e1dfe6255e603b6395d58e046e4ab827a5016f`; its ready receipt/source and complete
138-pass/54-child evidence were inspected before dependent work. Workspace/clone
instructions, finite policy/plan and exact U9 task were read. U9 implementation and
focused native acceptance are complete: **154/154 pass**, **all 138 U8 names/order
retained**, 12 new Engine cases, three actual public Engine consumer cases and one
new allocation probe. There are zero skips/unrun cases or ASan/UBSan diagnostics.
All **54 complete child transcripts are byte-identical** (21 exit 0/15 exit 2/18
exit 3). The ready receipt reports worker execution, not automatic source/platform
acceptance. This is the final assigned milestone of worker-4's finite batch.

## Implemented caller and owned observation

```cpp
#include <cheryl/core/game-framework/dialogue.h>
const auto offered = session.view(test).value();
const auto transition = session.choose_with_transition(offered.choices[0].selection, test, apply);
if (transition) {
    // Owns definition/revision, logical session, source/target, source-scoped
    // choice and old/new steps; no live incarnation or replay authority.
}
```

Transition owns five strings plus definition_revision and old_step/new_step. It
retains no DefinitionHandle, Session, handler, Selection/incarnation or app flag.
Copying or editing it cannot confer live authority, authenticate history, replay
an effect or supply durable delivery. A caller's later copy/logging/adoption
failure does not undo the committed choice. Definition/revision, logical session
ID, source/target and the choice scoped to source identify the observed occurrence;
new_step is old_step+1, including max-1 to max. Self-cycles keep equal endpoints
but require a new live selection for the next occurrence.

One private bool-template path performs both legacy and observing choices with
identical state/authority/membership/overflow/missing-handler/error ordering.
Validation through pure condition recheck and next-node/step preparation is
byte-identical to U8 (u9-source-audit.log SHA256
`be281af2eeffcefbf6d81254a9e331010ae925da24725cf29db79e4072d722e1`). Only the
opt-in variant copies observation fields and constructs the complete expected
success before effect entry. Non-const success returns by elision/implicit move;
Transition and expected<Transition,SessionError> have static nothrow-move checks.
After successful effect return only numeric cursor/step commit, result movement
and nonthrowing busy cleanup remain. Legacy choose returns expected<void,...>
without unused receipt allocation before or after effects.

Guard/preparation exceptions propagate with healthy unchanged progress and no
effect. Effect exceptions rethrow the original failure, fault the source cursor
and may leave partial app mutations; no receipt escapes and no rollback/retry is
claimed. Reentrant view/legacy choose/new choose/snapshot return Busy. Moved-from,
foreign/stale/wrong-incarnation, unavailable, missing-handler, max-step and terminal
cases retain their existing semantics. Borrowed handler arguments/keys remain
stable; callback moves/replacement/destruction remain prohibited. Start/view/
snapshot/resume never apply effects. Capture/adopt snapshots and app flags on the
same serialized owner as before. Structural advice is never live authority.

ST-003/005/019/020 in both decision views give contracts, confidence and alternatives.
Private compile-time variants avoid divergent copies of checks and wrapper success
preparation gaps, with no generic result framework. This is engineering judgment,
not new personal-taste evidence. Public dialogue.h stays CPU/JSON-free and Session
object data layout/legacy call syntax are unchanged. New Engine member/type and
focused consumer/probe wiring are the entire runtime change. Definition indices,
inspection/routes, private console/JSON/file logic, snapshot schema, pins and app
operation semantics stay unchanged. No event bus, automatic playback, global
registry, durable external-effect protocol or sibling dependency is added.

## Actual native checks

Reused builds/storytelling-u1-debug, preserving Debug/shared Engine, CMake policy
minimum 3.5, prior OFF selections/pins and local incremental outputs. Every
configure/build/native check used the shared one-job wrapper with unchanged adaptive
monitor/no-swap/admission limits. Original build/test sessions were retained through
completion. No native failure, resource refusal, cancellation/relaunch, sanitizer
or build-flag adjustment occurred. ASan/UBSan and halt-on-error remained enabled;
command-local detect_leaks=0 is the existing ST-008 tracing accommodation. Leak
coverage remains **unverified**. No broad campaign, GUI/device, SDK/doc build,
package download/install, protected/sibling cache, external agent or transport ran.

Executed commands from clone root (all immediate exits **0**):

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine tests-dialogue-inspection dialogue-lookup-allocation dialogue-transition-allocation dialogue-console tests-dialogue-console --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection|dialogue_lookup|dialogue_route|dialogue_transition)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.|tests-dialogue-(lookup|transition)\.allocation$)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection|dialogue_lookup|dialogue_route|dialogue_transition)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.|tests-dialogue-(lookup|transition)\.allocation$)'
/mnt/projects/cheryl-features/scripts/build -- nm -D --defined-only --demangle \
  builds/storytelling-u1-debug/libcheryl-engine.so.4.0.0
```

The full suite ran once with final source. Registration/actual results agree:
77 Engine GoogleTests, 11 independent public Engine consumer GoogleTests, 64 private
console GoogleTests, and two isolated allocation probes = 154. No source change
required a rerun or acceptance weakening.

| Files under builds/storytelling-u1-debug/ | Observed result |
| --- | --- |
| u9-configure.log, .exit | 0; preserved existing Debug/OFF selections and pins. |
| u9-build-1.log, .exit | 0; actual shared Engine, focused runners, public consumer and new standalone probe compile/link/discover. |
| u9-registration.log, .exit; u9-registration-audit.log | 0; 154 nonempty unique names, all 138 U8 names/order and all 16 new source/probe names match. |
| u9-ctest-1.log, .exit | 0; 154/154 pass in 18.15 seconds, zero skips/unrun cases. |
| u9-ctest-details-1.log | Full preserved LastTest.log, 152 matching RUN/OK pairs plus both probes, 154 CTest pass records, full child arguments/stdin/stdout/stderr/immediate exits. |
| u9-results-audit.log | Complete result/name/count agreement, all 54 child blocks exactly equal to U8, no ASan/UBSan diagnostics; new probe message records five preparatory failures and allocation-free commit/return/move/legacy path. |
| u9-source-audit.log | Selection validation/rejection ordering through next-step preparation byte-identical to U8; exactly one shared checked path. |
| u9-exports.log, .exit | Wrapped nm exit 0; new choose_with_transition and existing choose defined/exported by actual shared Engine; private choose_impl templates hidden. |
| u9-boundary-rules.log | Engine owns dialogue-session.cpp, consumer/probe import without private/app/JSON includes or export patch, actual consumer links Engine and executes new API; ASan/UBSan retained. |
| u9-audit-diagnostics.log | Static link-rule matcher initially raised StopIteration because CMake adds the discovery script as a second Ninja output; corrected matcher passes against the actual multi-output rule. No native check failed or reran. |

Logs/statuses stay ignored and clone-local. New receipt evidence references actual
nonempty regular native log files. Five changed C++ files pass clang-format dry-run
with Werror and git whitespace checks. Complete diagnostics are preserved despite
bounded tool output; no previous checkpoint/receipt is rewritten.

| New passing cases | Proven boundary |
| --- | --- |
| dialogue_transition.caller, silent | Both gate endings; complete literal observation, no-effect silent/leave/pass transitions, unavailable pass leaves pair unchanged, one introduction effect. |
| rejections, handler_order | Wrong session/incarnation/stale node/step/choice, unknown choice, missing handlers in old order, changed condition after available presentation; no effect or progress on rejection, legacy/new calls interleave. |
| guards, effects, reentrant | Original guard exception preserves healthy cursor and runs no present effect; original effect failure faults with partial app mutation, rejects further operations; both callback phases reject nested view/legacy/new/snapshot Busy and borrow stable keys. |
| moves, resume, cycles | Idle move construction/assignment preserve authority, replaced/moved-from/old-resume tokens reject, failed resume preserves original token, paired flags at gate resume/pass without repeated introduce, cycle needs fresh selection; edited observation fields grant no marker. |
| boundaries, retained | max-1 to max success, terminal-before-membership/overflow rejection, max-step prompt refuses effect, owned long/NUL/high-byte IDs after draft/definition/session release and independent copy. |
| dialogue_transition_consumer.caller, retained, authority | Actual public-header-first shared Engine call and owned fields, nothrow move contract, lifetime independence and same-field resume incarnation rejection/fresh success. |
| tests-dialogue-transition.allocation | Bounded 0..31 allocation-position seam reaches five failures then success; each failure preserves healthy snapshot/token and no effect. Denial enabled inside effect permits actual receipt return and further result move without new attempts; legacy choose and typed early rejects require no unused allocations from entry. Scalar/array/aligned and actual Engine hook controls verified; no OOM/exhaustion or general failure framework. |

## Final finite-batch receipt and next action

Committed receipt:
feature-development/queue/three-commits-20261024/STORYTELLING-U9.json,
exact batch/milestone bindings, ready/true with actual nonempty native evidence.
No unresolved assigned prerequisite/acceptance blocker remains. This is execution
evidence, not automatic coordinator source review or proof of an unrun platform.

The three assigned milestones are U7 (`dc946121`), U8 (`66e1dfe`) and this coherent
U9 checkpoint. Previous unrelated history is not counted. Next concrete action:
the finite controller verifies the drained clean final descendant/receipt and
ends this lane; the coordinator reviews source/evidence and the publisher owns
transport. No fourth unit is assigned or automatically selected. This worker
stops after U9, without starting a consumer expansion, sibling integration,
background/recurring task, publication or speculative outcome/event framework.

Confidence is high in executed ownership/authority/exception sequencing and
allocation-free post-effect movement, medium in new field/method spelling and
private compile-time organization. Reversing these choices needs source consumer,
error-order and allocation probes but no save migration. Broader platforms/custom
allocators, installed SDK/mixed-version ABI, leak detection and wider runtime
integration remain unverified. This probe covers C++ allocation on its serialized
thread, not arbitrary C/custom allocators or all fault positions. Receipts provide
no authenticated history, delivery guarantee, application rollback or external
exactly-once crash behavior.

## Retained U8 bounded advisory structural routes

STORYTELLING-U8 starts from clean executed U7
`dc94612134b11f03e3af9bdf91c8a442ca4eab29`; its committed ready receipt and saved
121-pass/54-child/allocation evidence were inspected before dependent work. The
finite policy, immutable plan and materialized U8 task were read. Implementation
and focused acceptance are complete: **138/138 checks passed**, **all 121 U7 names
retained in order**, 14 new Engine route cases and three actual shared-Engine
consumer cases. There are zero skips/unrun cases or ASan/UBSan diagnostics. All
**54 full child transcripts remain byte-identical** (21 exit 0/15 exit 2/18 exit 3).
The ready U8 receipt is worker execution evidence, not automatic source/platform
acceptance. Only this one U8 checkpoint is assigned in this invocation.

## Implemented caller and semantics

```cpp
#include <cheryl/core/game-framework/dialogue-route.h>
const auto route = CE::Dialogue::find_route(*definition, {"start"}, {"end"}, {});
// Literal graph: route->nodes = start,z,end; route->choices = c0,c0.
// choices[i] is scoped to nodes[i], targeting nodes[i+1].
```

Route owns definition/revision, ordered node IDs and source-scoped choice IDs,
plus usage. FIFO BFS and first discovery resolve shortest-edge ties in authored
choice order, independent of sorted IDs, authored node order or entry position.
Known equal endpoints return one node/zero edges. Self/parallel/conditional edges,
cycles and terminal-free components are valid. UnknownFrom/UnknownTo/Unreachable
are separate typed failures. There is no nullable handle overload; a live
Definition reference is required. Structural advice proves no gameplay availability,
effect history, resume authority, termination or completion. No handler, predicate,
effect or Session is accepted/constructed/called by the operation. The session
activity test deliberately shows a structurally advised edge remaining unavailable
under an unchanged live application predicate/cursor.

Use one 3*N size_t scratch allocation (parents, parent-choice ordinals, FIFO),
checked arithmetic and preflight of all authored nodes/choices, even unreachable
ones. U7 indexed lookup reserves `(bit_width(N)+1)*(1+query_bytes)` before each
query: a conservative comparison/byte bound, not measured prefix work or a timing
promise. Search is O(N+E) visits plus O((E+2)*log(N+1)*L) reserved lookup byte work
and owned copies. A future core lookup change must revalidate that bound.

RouteLimits defaults: 256 nodes, 2048 choices, 255 output edges, 128 KiB repeated
output-ID bytes, 1 MiB requested payload, 128 Mi work units. Absolute request
ceilings: 4096/65536/4095/4 MiB/16 MiB/1 Gi. Independent zero budgets permit none;
above-ceiling/size_t-maximum requests return InvalidLimits before traversal or
allocation. Larger core definitions than private JSON caps can use higher allowed
budgets. Output-edge limits reject the discovered shortest route without truncating
search or claiming unreachability. ID/output/storage/copy-work checks precede result
allocation. Payload is `3*N*sizeof(size_t)+sizeof(Route)+(R+1)*sizeof(NodeId)
+R*sizeof(ChoiceId)+copied_id_bytes+2*R+2`; allocator metadata/rounding, excess string
capacity, fixed stack and borrowed definition/index storage are excluded. This is
not RSS containment. Allocation exceptions propagate; impossible broken compiled
target membership throws logic_error. Neither refusal nor exception publishes a
partial route or changes a definition/previous result.

ST-017/018 in both decision views give exact precedence, units, ownership,
alternatives, confidence and reversal cost. These are engineering choices, not
new user API taste. Engine owns the new source through its normal glob and CE_API
boundary. The independent public-header-first consumer links actual Cheryl::Engine;
no private/app/JSON includes, export patch, source copy or alternate library exists.
Existing definition/inspection/session and private CLI/JSON/file implementations
are unchanged. Two obsolete detached U7 linear-lookup comments are corrected to
its already-implemented indexed semantics, with no lookup behavior change.

## Executed native verification

Reused builds/storytelling-u1-debug and preserved Debug, shared Engine, CMake
policy minimum 3.5, pins, all OFF selections and clone-local incremental outputs.
Every configure/build/native command ran through the shared wrapper with one job
and the unchanged adaptive monitor/no-swap/admission policy. The original CTest
registration session waited in the normal queue and completed; it was not cancelled
or relaunched. No resource refusal, build/test failure or sanitizer/configuration
change occurred. A premature **static** name audit read the still-empty discovery
log; its exact AssertionError and correction are saved. Native work was not retried.

ASan/UBSan instrumentation and halt-on-error remained active. Invocation-only
ASAN_OPTIONS=detect_leaks=0 is the already-confirmed ST-008 tracing accommodation;
leak coverage stays **unverified**. No broad suite, GUI/device, installed SDK,
documentation generation, package/dependency retrieval, protected/sibling cache,
subagent/external agent or publication transport was used.

Actual commands from the clone root (all immediate exits **0**):

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine tests-dialogue-inspection dialogue-lookup-allocation dialogue-console tests-dialogue-console --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-dialogue-inspection --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection|dialogue_lookup|dialogue_route)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.|tests-dialogue-lookup\.allocation$)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection|dialogue_lookup|dialogue_route)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.|tests-dialogue-lookup\.allocation$)'
```

Second build strengthens only consumer assertions (check error-state before error()
and pointer before dereference, plus nonnullable-header proof), before any CTest
execution. No tests needed rerunning after implementation changes: all 138 ran
once with final source. Checks discover 65 Engine GoogleTests, eight public consumer
GoogleTests, 64 private console GoogleTests and one isolated allocation probe.

| Files under builds/storytelling-u1-debug/ | Actual evidence |
| --- | --- |
| u8-configure.log, .exit | 0; existing configuration choices retained. |
| u8-build-1.log, .exit; u8-build-2.log, .exit | 0; real Engine/new public route and focused runners compile/link/discover. Second build updates only strengthened consumer checks. |
| u8-registration.log, .exit; u8-registration-audit.log | 0; nonempty 138 names match final source order; all 121 old names/order retained, 17 new. |
| u8-ctest-1.log, .exit | 0; 138/138 passes, 17.34 seconds, no skipped/unrun cases. |
| u8-ctest-details-1.log | Full preserved LastTest.log: 137 matching RUN/OK pairs plus allocation probe, 138 CTest pass records and every child transcript/immediate exit. |
| u8-results-audit.log | Names/results/counts agree; all 54 complete child blocks equal U7 byte for byte; no ASan/UBSan diagnostics. Core definition/session/inspection and private CLI implementations unchanged. |
| u8-boundary-rules.log | Real shared Engine owns route.cpp.o with export flag; consumer imports without private/app/JSON wiring and executes dladdr/equivalent against that exact library; both instrumented. |
| u8-audit-diagnostics.log | Exact premature static name-audit failure, empty log while registration session was live, then successful dependent audit after its exit 0. No native failure/relaunch. |

Full logs/statuses are ignored clone-local outputs; the committed receipt references
nonempty actual native evidence. Five changed C++ files pass clang-format dry-run
with Werror and git whitespace checks. No old receipt/history is rewritten.

| New passing cases | Meaningful coverage |
| --- | --- |
| dialogue_route.caller, ties, shortest | Literal ordered route and scoped choices; nonfirst/nonentry endpoints; shorter later branch beats first longer branch; nonlexical authored ties, swapped tie, self and conditional parallel first edge. |
| cycles, equal, endpoints | Cyclic/terminal-free components, unreachable isolated terminal and reverse terminal path; known prompt/terminal/self equality with zero output edges; empty/case/prefix/missing IDs and source-first diagnosis. |
| owned, advisory | 4096-byte/NUL/high-byte identities and owned definition/revision/choice strings after draft mutation/destruction and final owner release; structural edge remains disabled in a live unchanged session with zero effects. |
| budgets, payload, invalid_limits, larger, lookup_work | All six exact budgets and each one-short/zero refusal; literal payload; post-search ID/storage refusal preserves old output; hard+1 and size_t-max requests; 257 nodes/2049 choices with raised allowances; long known/unknown queries consume reserved work. |
| reference | 512 endpoint queries across eight bounded independent graphs compared with a separate linear-name/queued-owned-path BFS reference, including complete ordered choices. |
| dialogue_route_consumer.caller, retained, refusals | Public-header-first real shared Engine execution/export owner; owned output after definition release; exact/short/invalid budgets and all endpoint/no-route errors without altering earlier route. |

## Receipt, remaining gaps and next action

Committed receipt:
feature-development/queue/three-commits-20261024/STORYTELLING-U8.json,
exact batch/milestone bindings, ready/true with actual nonempty native log paths.
No unresolved assigned prerequisite or acceptance blocker remains. This reports
worker execution, not automatic coordinator source acceptance or an unrun platform.

Next concrete action: finite queue controller checks the clean descendant
checkpoint/receipt after this run drains and selects the preassigned U9 owned
transition-observation task. This worker stops after U8; no U9/background/sibling
unit, integration, publication or recurring scheduling starts here.

Confidence is high in BFS/ownership/checked bounds and executed shared imports,
medium in public spelling/defaults/ceilings and conservative work accounting.
Changing those choices requires consumer/budget checks and matching builds; no
persisted data migration is introduced. Leak coverage, other platforms/allocators,
installed SDK/mixed-version ABI and wider runtime integration remain unverified.
Allocator/programming exception paths are specified/source-inspected, not exhaustively
failure-injected; payload budgets exclude allocator overhead and there is no timing
benchmark or gameplay availability/history/termination claim.

## Retained U7 immutable dialogue lookup indices

STORYTELLING-U7 starts from clean, coordinator-accepted U6
`4bb94118413233cc476fac3938f1af75573765f7`. Only this milestone in finite batch
three-commits-20261024 is active. Implementation and focused acceptance are
complete: **121/121 checks passed**, including **all 112 retained names in order**,
six new Engine cases, two public-consumer cases and one isolated allocation probe.
There were no skipped/unrun checks or ASan/UBSan diagnostics. All **54 complete
child transcripts match U6 byte for byte** (21 exit 0 / 15 exit 2 / 18 exit 3).
A ready/true receipt accompanies this one coherent U7 checkpoint; it is worker
execution evidence, not automatic coordinator source/platform acceptance.

## Implemented caller and contract

```cpp
#include <cheryl/core/game-framework/dialogue.h>
const auto* choice = definition->find_choice({"gate"}, {"pass"});
// The exact authored Choice is borrowed; retain definition while using it.
```

The caller/public signatures and noexcept are unchanged. Validation runs in its
original order, then Definition deep-copies the draft and privately sorts node
ordinals and node-scoped choice ordinals. Authored vectors/strings/order, entry
ordinal, pointer identity/lifetime and independent same-named choices across nodes
are preserved. Lookup uses exact std::string lower_bound/equality without key
copies, allocation, normalization or lazy mutation. Long/binary IDs supported by
core remain valid. Construction/index allocation may throw before a usable const
Definition is published, with ordinary RAII cleanup. ST-003/016 in both decision
views record the ownership, memory/comparison tradeoffs and confidence.

For N nodes/Ci choices, retained index element payload is
`(N + sum(Ci))*sizeof(size_t)` plus per-node vector objects/capacity/allocator
metadata. Sorting uses O(N log N + sum(Ci log Ci)) string comparisons; lookup uses
O(log(N+1) + log(Ci+1)) comparisons, each with length-dependent byte work. There is no
constant-time claim or timing campaign. Existing Session/inspection implementations
and inspection's charged manual target comparisons are unchanged; constructor/index
storage remains outside its per-call payload/work budgets. Public declaration
signatures are stable, while private Definition layout requires matching-header
Engine/consumer rebuilds. Mixed old binaries or installed SDK ABI are unverified.
No JSON/file/flag/handler/live-authority policy or operation semantics changed.

## Actual native checks

The finite policy, immutable batch plan, current worker task, workspace/clone
instructions and accepted U6 records/checkpoint were read before edits. No
pre-existing changes or unresolved U6 prerequisites existed. The preserved
GNU/Linux Debug shared-Engine build, CMake policy minimum 3.5, previous OFF options,
pins and incremental outputs were reused. Every configure/build/native check used
the shared one-job wrapper. The original queued build session was retained through
normal admission and completion; no cancellation/relaunch or resource exit 75
occurred. Adaptive monitor controls, no-swap and 4 GiB admission floor were unchanged.

ASan/UBSan instrumentation and halt-on-error remain enabled. Invocation-only
ASAN_OPTIONS=detect_leaks=0 is ST-008's established tracing accommodation; leak
coverage remains **unverified**. The Linux-gated standalone probe never links its
replacement operators into GoogleTest. Scalar/array/aligned rejection controls and
one allocation-bearing Engine compile fail deterministically before **any lookup**;
this verifies shared-library interposition too. Then 2300 hits/misses/terminal
queries run under denial with no new rejected allocation. Owning query values and
expected authored addresses are prepared before denial. No OOM, RAM pressure,
arbitrary C/custom allocator instrumentation or timing test is claimed.

Actual commands from clone root (all exit **0**, complete logs/immediate statuses):

```sh
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake -S . -B builds/storytelling-u1-debug
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target tests-engine tests-dialogue-inspection dialogue-lookup-allocation dialogue-console tests-dialogue-console --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- cmake --build builds/storytelling-u1-debug \
  --target dialogue-lookup-allocation --parallel 1
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  -N -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection|dialogue_lookup)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.|tests-dialogue-lookup\.allocation$)'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir builds/storytelling-u1-debug \
  --output-on-failure --no-tests=error --parallel 1 \
  -R '^(tests-engine\.(dialogue_definition|dialogue_session|dialogue_inspection|dialogue_lookup)\.|tests-dialogue-console\.dialogue_console_|tests-dialogue-inspection\.|tests-dialogue-lookup\.allocation$)'
```

The second build adds the shared-Engine rejection control after review, before
any native test execution. No implementation defect or sanitizer/config change
was needed. All tests ran once with the completed probe.

| Saved clone-local files under `builds/storytelling-u1-debug/` | Observed result |
| --- | --- |
| `u7-configure.log`, `.exit` | 0; retained Debug/OFF selections and pins. |
| `u7-build-1.log`, `.exit`; `u7-build-2.log`, `.exit` | 0; matching-header real Engine, focused consumers/probe and console runners compiled/linked. Second build strengthened only the probe control. |
| `u7-registration.log`, `.exit`; `u7-registration-audit.log` | 0; 121 nonempty names match source/registration order; all 112 U6 names retained in order, 9 new. |
| `u7-ctest-1.log`, `.exit` | 0; 121/121 passed, 16.19 seconds, no skips/unrun checks. |
| `u7-ctest-details-1.log` | Complete preserved LastTest.log: 120 GoogleTest RUN/OK pairs plus standalone probe success, 121 CTest passes, all 54 full child arguments/stdin/stdout/stderr/immediate exits. |
| `u7-results-audit.log` | Counts/names/pass records agree, all 54 child blocks byte-identical to U6, no ASan/UBSan diagnostics, allocation-probe success/control evidence. |
| `u7-source-audit.log` | compile validation/reporting body byte-identical to U6, SHA256 `267fe629e9c758f5902cd9804d31668d6040e226cfbf7cddd1b37200bf20327e`. |
| `u7-boundary-rules.log` | Actual Engine owns changed lookup implementation/export flags; public consumer/probe have no private/app/JSON includes or export patches; ASan/UBSan retained. |
| `u7-checkpoint-audit.log` | Final static audit corrected assumptions about CTest summary wording and number padding; both AssertionErrors are saved. Corrected matchers verify receipt, exits, counts and staged scope. No native check failed or reran. |

Logs/status files remain ignored and clone-local, with no source writes/sidecars or
protected/sibling cache use. GTest registration reuses the Engine unit source glob
and existing actual-Engine consumer suite; the allocation probe has its own Linux
executable/CTest entry and inherits the wrapped native envelope. All five changed
C++ files pass repository clang-format dry-run/Werror and whitespace checks.

| New passing checks | Observable boundary |
| --- | --- |
| dialogue_lookup.shuffled, misses | 17 permuted authored nodes/31 permuted choices per prompt; all exact hits/addresses/order, nonfirst entry, before/after/prefix/case/empty/missing-node/terminal misses and a single terminal graph. |
| binary_ids | 4096-byte common prefixes, exact prefix IDs, embedded NUL/high bytes and near misses; full bytes remain distinct. |
| ownership, scope | Draft mutation/destruction, retained handles/weak owner expiry, stable original node/choice pointers, independent repeated choice names across nodes and exact scoped-duplicate rejection. |
| diagnostic_order | Eleven literal owned diagnostics preserve complete authored traversal after source destruction, including supported-name, entry, scope, shape and unreachable errors. |
| dialogue_lookup_consumer.caller, retained | Unchanged header-first find_choice caller through real Cheryl::Engine, original authored pointer/order, scoped choices/misses and retained long/binary names. |
| tests-dialogue-lookup.allocation | First-use and repeated valid/missing/terminal queries under verified scalar/array/aligned/shared-Engine allocation denial, with no extra attempts and stable authored pointers. |

## Receipt, limitations and next action

The committed receipt path is
`feature-development/queue/three-commits-20261024/STORYTELLING-U7.json` with exact
batch/milestone bindings, status ready, checks_passed true and nonempty relative
paths to the actual saved native logs. Only this U7 implementation/records/receipt
form the new checkpoint; prior history is not counted toward the three-milestone
batch. No unresolved assigned blocker remains.

Next concrete action: the finite queue controller checks the clean descendant
checkpoint/receipt and selects preassigned STORYTELLING-U8 after this run drains.
This worker stops after U7; it does not start U8, a sibling unit, background agent
or publication transport. The next dependent worker must inspect this checkpoint
and its executed evidence; the receipt is not automatic coordinator source review.

Confidence is high in executed ownership/exactness/no-allocation behavior, medium
in per-node-vector memory layout. This is an engineering choice, not new user taste
evidence. A private layout alternative is locally reversible with matching Engine/
consumer rebuilds and these checks. Leak detection, broader platforms/allocators,
installed SDK/mixed-version ABI and wider runtime integration remain unverified.
Lookup byte work is length-dependent; no performance benchmark is claimed.
No broad campaign, device/GUI, documentation/SDK build, package install/download,
pin/global/resource configuration change, subagent or external agent ran.

## Retained U6 reusable bounded Engine inspection

STORYTELLING-U6 started from clean accepted U5
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
