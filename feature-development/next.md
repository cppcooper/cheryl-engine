# U4 bounded authored definitions

Current assignment STORYTELLING-U4 starts from clean accepted U3
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
