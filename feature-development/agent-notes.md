# MAGIC-M1 detailed engineering record

## Assignment and source boundary (before implementation)

Fresh worker-3 MAGIC task, not the archived map-storage queue. Starting HEAD
3542f883391d4225329dab8d5d61a5987e69c8f0, clean feat/magic. No existing
feature-development records or native outputs. The workspace worker record
runtime/workers/worker-3/status.json identifies run 20261024T024553Z-e2b3aa5a6271,
thread 01a16bcc-ecd0-7530-bace-9958eddb5e0a, reasoning_effort ultra and admitted
resource scope. Model selection is inherited; no substitutions. Workspace rules,
autonomy policy, worker assignment, backlog, brief, MAGIC design and decisions
are inspected. The three exploration reports are 27 alternative strategies,
not branches, experiments or passing implementations. Selected synthesis owns
fixture numbers and M1 scope. No sibling merge, dependency or mutation.

## Proposed ordinary caller (recorded before dependent APIs)

```cpp
Magic::Catalog game; // host supplies supported primitive schemas and keys
// Populate game with Heal -> game.heal, Fire -> game.fire, Slow -> game.slow.
auto result = Magic::compile(edited_text, game);
if (result) {
    accepted = *result; // shared_ptr<const Plan>, replace only on success
    std::cout << accepted->common() << '\n' << accepted->inspection();
} else {
    std::cerr << result.error().message; // owns code, span, excerpt and advice
}
// accepted survives edited_text, result and catalog destruction.
```

Private proposed names, not a public Engine API or user preference. Const plans
expose typed meaning; compilation/inspection never calls a host or executes effects.
Controlled Common is a grammar, not arbitrary English. Source selects semantic
primitives; only host schemas map those primitives to symbolic game keys.

## Literal normalized baseline (recorded before implementation)

All spatial values use named world frame G0, integer mm; time is integer 10 ms
ticks. Actor-centre disk boundary is inclusive. These are local prototype units.

| Fixture | Independent expected semantic values |
| --- | --- |
| S1 | spoken/release, contact, onset 0 ticks; binding hand = one willing touched actor selected at release, no fallback; [game.heal 12 hp, instant, hand]. |
| S2 | spoken/release, burst, onset 100 ticks; binding near = eligible actors in G0, disk radius 250 mm inclusive; aim captured at release, membership queried once at activation; ordered together [game.fire 8 dp instant near, game.slow 750 permille for 200 ticks near]. |
| S3 | spoken/release, burst, onset 0 ticks; same 250 mm near query; [game.slow 750 permille for 200 ticks near]. Exact contextual 25 framebuffer px at 10 mm/px, C7 revision 3, plane G0 has equivalent world meaning; absent/unsupported context refuses. |
| S4 | identical meaning to S2; Cinder@1 postfix fire and Mend@1 verb-first slow clauses cross explicit on near bridge boundaries. Mend uses recipient marker tava-a; Cinder slow additionally has duration-first order. |
| S5 | engraved/item-touch, exactly one required charge (metadata only), item-event delivery, onset 0; binding toucher = willing accepted-event toucher at activation; [game.heal 12 hp instant toucher]. No installed item, delivery, charge consumption or persistence. |

Independent Common literal readings live with test fixtures; canonical rendering
uses integer milliseconds (1000 ms onset, 2000 ms slow). Seconds are accepted
exactly, never rounded. Common round-trip promises equivalent typed meaning with
the same host catalog; source language, spans and px conversion provenance are
excluded from semantic equality. Author order and target binding identities remain
observable. Sequential then and conflicting duplicate fields refuse.

## API evidence and confidence

- Intentional v3 source eb3f81c (V3.0a #12), input-binding.h: separate digital and
  analog role types with explicit constructors; input-mapper.h explicitly calls
  its own purpose tentative ("I think. 99% sure"). Evidence for explicit roles,
  counterevidence against treating every imported abstraction as settled taste.
- Josh Cooper commit 4e40aa1cb309808ff7d9976c61228d1db7445e01, math/time.h:
  chrono dimensions and compile-time cast constraints. Supports distinct units;
  does not select our mm/tick numbers or justify lossy conversion.
- Josh Cooper commit 18eac46e8e070ad79554f459943462fb26442093, templates/block.h:
  shared owners and optional result with explicit bounds; evidence for lifetime
  reasoning, not a mandate to reuse that container or its public mutable fields.
- Modern CMakeLists.txt selects C++23; .clang-format owns formatting. Current
  first-game main.cpp is a compatibility example, not proof of personal taste.
- Storytelling U9 4f5e7eab4af961d95f5a04d6f4f079517e39a758 read via git show
  (dialogue.h, dialogue-session.cpp): immutable copied definitions, symbolic keys,
  owned errors/observations and live condition recheck. Prepare-before-effects
  helps later host design. Its graph is not a spell VM, its receipt is not durable,
  callback exceptions can leave application effects partial. No import or merge.

## Implementation sequence and acceptance map

1. Own syntax -> typed validation -> immutable meaning; render exact Common and
   role/key inspection. Malformed/ambiguous text must not replace previous results.
2. Two invented school grammars and explicit lexical binding; immutable artifacts
   must survive all input owners. Quantity/time/delivery/binding edits need witnesses.
3. Headless stdin/file inspection plus focused independent literal tests and small
   ordinary caller scenarios, not helper-only assertions.
4. All configure/build/native executions use workspace scripts/build --, one job,
   incremental clone-local build output, complete saved logs. Record refusals and
   stop native admission attempts on unchanged resource/allowance blocks.
5. Review staged diff, update compact/detailed records, next action and JSON receipt;
   local command-identity commit. Publisher owns remote backup.

## Reversible scope decisions

M1-013: explicit nonrecursive envelope and flat bindings, one activation and at
most four ordered effects. Common@1, Mend@1 and Cinder@1 are trusted handwritten
microgrammars. All school names/words are invented, not Elvish. Future language
packs need their own checked grammar; no provider/parser network calls.

M1-014: contact and instantaneous ray delivery can address the same touched-actor
query. This small additional delivery lets an edit change only delivery meaning;
the willing/touched predicate remains mandatory, ray path/occlusion is a later
host check. Burst accepts disks and item-event accepts an engraved event toucher.
No projectile travel, arbitrary combinations, scheduler or general World.

M1-015: limits are caller-tightenable below fixed ceilings. Source, tokens,
brace nesting, bindings, clauses/effects, numeric ranges, work and retained logical
storage have independent checks. Retained storage is deterministic object slots
plus owned text bytes including terminators, not allocator bookkeeping. Fixed
ceilings also bound vector/string capacity and temporary parsing storage. One
owned diagnostic has a separate small fixed reserve so a zero budget can explain
its failure. Exact numerical details and executed evidence follow after checks.

During grammar implementation, S4 uses Cinder fire plus Mend slow to make the
normalized S2 payload explicitly bilingual across two schools. The numeric and
target baseline is unchanged. Mend luma is a verb-first finite slow production;
Cinder neth remains duration-first. Neither is unrestricted lexical substitution.

## Implemented private contract

`projects/magic` is a standalone C++23 project using the standard library. It
changes no Engine target, sibling, submodule pin or dependency checkout. The
compiler has no provider, network, random, world, scripting or effect callback
path. `magic_inspect [VERSE_FILE]` accepts an editable file or stdin and emits
inspection plus exact Common; it rejects input above 8192 bytes while reading.
Its fixture host supplies `game.heal`, `game.fire` and `game.slow`. Other callers
supply their own Catalog. `tests/caller.cpp` uses distinct host keys and ordinary
calls, without the implementation or contract-test helper routines.

The compiler lexes checked tokens/spans, parses a private borrowed `Authored`
syntax tree, then validates dimensions, typed roles, binding references, host
schemas and combinations while constructing owned `Meaning`. Only successful
compile exposes a `shared_ptr<const Plan>`. The Plan constructor is private;
copy/move construction and assignment do not offer mutation. Source, binding/frame
names, symbolic keys, grammar/span provenance, conversion provenance, exact Common
and inspection are owned. Accessors borrow the plan's lifetime; copying its handle
retains everything after source, catalog, options and ephemeral compiler destruction.
Stable reads are concurrent; inputs must remain stable during each synchronous
compile. Validation failures own one diagnostic. Allocation exceptions propagate
without modifying inputs or previously accepted handles. Host permission is never
conferred by compiling a word or retaining a plan.

Entry medium (`Spoken`/`Engraved`), casting mode (`Release`), trigger
(`None`/`ItemTouch`), charge requirement, delivery, onset, query/binding timing and
effect duration are separate typed data. Release is the only supported mode;
wind-up, channels, enchantment envelopes and richer triggers need later contracts.
Engraving explicitly reads `engraved item-touch charges 1 release`. The shared
payload is independent of entry media. No item is installed and no charge/event
is consumed, replayed or persisted.

### Grammar and exact readings

ASCII case-sensitive words and punctuation, with whitespace between quantities and
units. `NAME` is 1..32 bytes, starting with a letter, then letters/digits/_/-.
Each `Q` below is an unsigned decimal with at most three fractional places plus a
unit; typed validation rejects the wrong dimension and any inexact normalization.
There are no aliases for arbitrary English, name guessing, macros or recursive
language islands.

```text
verse := entry via delivery after Q { binding+ together ordered { clause+ } }
entry := spoken release | engraved item-touch charges 1 release
delivery := contact | ray | burst | item-event
binding := bind NAME = willing touched actor at release ;
         | bind NAME = willing event toucher at activation ;
         | bind NAME = eligible actors in FRAME disk Q around aim captured
                   at release queried at activation inclusive ;
clause := on NAME bridge LANGUAGE @ 1 { payload } ;
Common payload := heal Q instant
                | fire damage Q instant
                | multiply speed by Q for Q
Mend payload := sela Q tava-a | luma Q tava-a lasting Q
Cinder payload := Q hara | Q Q neth
```

Mend `sela` is healing; `luma` is finite speed scaling. `tava-a` marks the actor
recipient exported by the outer bridge, including an actor set. Cinder `hara`
is fire with quantity before the verb; `neth` has duration first, rate second,
verb last. These invented subsets differ in word order and role markers; the
parser does not replace strings into Common. S4 combines Cinder fire and Mend
slow and equals the independent S2 Common meaning. `mixed-schools.verse` composes
a previously unauthored delayed healing-plus-slow payload. A healing disk also
compiles without a named whole-spell handler.

During contract review Common becomes explicitly `fire damage` and `multiply
speed by N permille`, avoiding ambiguity between a multiplier and a reduction.
750 permille means 0.750 times prior speed, not a 750-permille subtraction. Effect
impulses have duration zero; status duration never repeats fire or changes onset.
The normalized numeric baseline remains unchanged. Independent canonical strings
in `tests/literals.h` and full typed literals in `tests/compiler.cpp` exercise all
five fixtures. Common reparse under the same host catalog promises equal Meaning;
provenance and original spelling are intentionally separate. Authored effect order,
binding names/indices, queries, frame, radius and timing remain semantic data.

### Supported combinations and authority

| Entry and delivery | Query and timing |
| --- | --- |
| Spoken release, contact or ray | One willing touched actor per binding, selected at release, onset zero. Ray is immediate; real ray/contact/occlusion checks remain host work. |
| Spoken release, burst | Eligible actors in a named 2D world disk, inclusive actor-centre radius; fixed aim captured at release, membership queried once at activation; onset 0..8,640,000 ticks. |
| Engraved release, item-event | Willing accepted-event toucher, resolved at activation; exactly one required charge, onset zero; metadata only. |

Effects in one binding share one selection. Separate bindings keep independent
identity and may overlap; no disjointness or live acquisition is inferred. Tests
show both independent identities with identical queries and routing slow to a
second 500 mm query while fire retains its original 250 mm query. Reversing the
fire/slow clauses changes effect order and exact Common. `then` rejects explicitly.
On one binding, repeated speed fields or multiple health fields (including
heal+fire) reject as unsupported interactions. Distinct bindings require later
live overlap/conflict checks. Nothing silently treats ordered together as a
sequence or grants atomic external effect delivery.

Host schemas bind each known primitive to one unique symbolic key, 1..64 ASCII
letters/digits/._- bytes. At most three schemas, unique by both primitive and key;
unknown enum values reject. Catalog capability flags govern medium/delivery and
multi-effect bundles, and per-schema quantity/duration maxima can tighten bounds.
Unknown words, unsupported recognized operations and unsupported combinations
have distinct diagnostics. Frame names describe required host roles; M1 never
looks up a real frame, actor, aim point, eligibility policy or live item. Later
hosts must honor primitive meaning and recheck current support and permission.

`px` is specifically framebuffer pixels. The optional authoring context needs a
matching world frame, named camera/revision and plane, a positive bounded rational
mm/pixel scale and an explicit uniform orthographic assertion. Perspective,
nonuniform, mismatched, missing or inexact contexts refuse. C7 revision 3, G0,
10/1 mm per px is supplied only in caller checks, never a compiler/CLI default.
25 qualified px equals 250 mm; the plan freezes world radius and owned provenance.
Common emits mm, so reparsing resolved meaning needs no camera. Context and draft
mutation do not change an accepted plan.

### Bounds and diagnostic contract

| Dimension | Fixed ceiling / accepted numeric domain |
| --- | --- |
| Source | 8192 bytes, including whitespace; lexer checks before scanning. CLI bounds reads too. |
| Tokens / brace nesting | 512 / 8; no recursive parser or unbounded expansion. Current valid envelope depth is 3. |
| Bindings / clauses / effects | 8 / 4 / 4, checked before pushing syntax/storage. One effect per bridge clause, at least one binding and effect. |
| Work | 65,536 deterministic meter units; scanned bytes, token operations, numeric digits, schema/reference/composition checks and owned copying/rendering are charged. Other per-step helpers are bounded by fixed identifier/schema/numeric widths. This is a work model, not CPU cycles or a real-time deadline. |
| Retained logical storage | 32,768 bytes: sizeof Plan + binding/effect/provenance element slots + owned text bytes and terminators. Checks precede copying, reserve and output append. Excludes unused container capacity, shared control block and allocator bookkeeping; sizeof makes totals ABI dependent. Fixed ceilings bound those extra allocations and temporary tokens/AST too; no portable exact heap/RSS ceiling is claimed. |
| Healing/fire / disk radius | Positive integer 1..1,000,000 hp/dp/mm, also limited by host schema for effects. |
| Speed multiplier | Integer 1..1000 permille, also host-limited; duration 1..8,640,000 ticks. |
| Onset | Integer 0..8,640,000 ticks (10 ms each), further constrained by delivery. |
| Context scale | Positive integer numerator and denominator, each <=1,000,000; revision is nonzero uint32. |
| Diagnostics | One owned code, original byte offset/length, excerpt <=64 bytes and fixed repair message (current messages <=200 bytes). Separate fixed-size error reserve, including when work/storage budget is zero; EOF/global errors can have zero-length spans. |

Limits can only tighten ceilings, including zero. The lexer and parser stop at the
first error; diagnostic order follows syntax before schema/lowering. All numeric
multiply/add checks happen before arithmetic; exact rational cross-cancellation
precedes multiplication and division. Slot-size multiplication uses counts already
bounded to 8/4. Quantities are validated before any size could depend on them.
No geometry/array allocation is derived from an authored radius or duration.

Tests accept and reject both sides of every configurable budget, including zero.
Absolute 8192-byte input, eight bindings and four effects/clauses have valid
witnesses. Token 512 and nesting 8 also have lexical boundary witnesses that then
fail grammar; valid sources use tightened exact token/depth boundaries. Numeric
maxima and one-past/zero/inexact/overflow cases are exercised. For the canonical
S2 literal on this ABI, exact work=2610 and logical storage=2700 pass; one less
fails with the specific budget code. This differs from source fixtures because
whitespace, language and millisecond spellings affect metered text.

## Executed verification and saved complete logs

All commands run from this clone. Every configure/build/native test passes through
`/mnt/projects/cheryl-features/scripts/build --`; no parallel child builds or sibling
caches. All configurations reuse `projects/magic/build` incrementally. No lock
cancellation, resource refusal, allowance workaround, download or install occurs.
GNU 16.2.1 on GNU/Linux, CMake 4.4, Python 3.14.7; Debug, C++23, -Wall -Wextra
-Wpedantic -Werror, ASan+UBSan. Generated flags confirm sanitizer instrumentation.
Leak checking remains unverified because LSan refuses tracing; final test command
disables leak detection locally and makes UBSan fail immediately on a finding.

| Actual command | Complete log(s), result |
| --- | --- |
| `/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug` | `magic/logs/configure.log`, `configure-02.log`; both exit 0. Second configuration registers the CLI check and discovers existing Python; no download. |
| `/mnt/projects/cheryl-features/scripts/build -- cmake --build projects/magic/build --parallel 1` | `magic/logs/build-01.log` through `build-05.log`; all five exit 0. Changes clarify Common, format code, make mode/trigger explicit and update affected literals. |
| `/mnt/projects/cheryl-features/scripts/build -- ctest --test-dir projects/magic/build --verbose --parallel 1` | `magic/logs/checks-01.log`; exit 8, 8 LSan tracing failures. No acceptance inferred. |
| `ASAN_OPTIONS=detect_leaks=0 /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir projects/magic/build --verbose --parallel 1` | `magic/logs/checks-02.log`; exit 0, initial 8/8 CTest entries. Superseded wording/field layout. |
| `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir projects/magic/build --verbose --parallel 1` | `magic/logs/checks-03.log`; exit 8, 8/9 entries. One S5 literal omitted its newly explicit ItemTouch field and is corrected to the original item-touch baseline; compiler behavior is unchanged by that correction. Same command in `checks-04.log` exits 0, **9/9 entries**. |

Final acceptance: **35 contract cases**, **4 independent ordinary caller scenarios**,
**6 direct file CLI fixtures** (five normalized plus novel mixed healing/slow), and
**9 CLI process scenarios** (stdin parameter/delivery composition, exact/above byte
ceiling, owned unknown/ambiguous diagnostics, help, missing file, excess arguments).
These comprise 9 CTest entries; counts are separate categories, not 54 CTest tests.
All five independent full Meaning and Common literals pass compilation, inspection
and round-trip. Parameter, bilingual, scope/order, ownership, diagnostic, operation,
unit/dimension/range and exact budget cases pass. No sanitizer findings appear in
the final logs. Static format, staged whitespace and receipt/evidence checks are
recorded in `magic/logs/static-review.log` and `staged-review.log` at finalization.

| Actual fixture source | Tokens | Depth | Work | Retained logical bytes |
| --- | ---: | ---: | ---: | ---: |
| S1 | 35 | 3 | 1839 | 2024 |
| S2 | 64 | 3 | 2590 | 2693 |
| S3 | 50 | 3 | 2211 | 2289 |
| S4 | 61 | 3 | 2545 | 2670 |
| S5 | 38 | 3 | 2004 | 2147 |

## Handoff and limits

M1 is ready for coordinator review, not production casting. No immediate fake host
is included: compilation/inspection gives the coherent proof without pretending
delays, live targets, charges or gameplay effects execute. Real target acquisition,
scheduling, mana/balance/permissions, overlap/interaction laws, event authenticity,
charge commits, durable state/restart and exactly-once external effects are pending.
Graphics, learner progression, a spellbook/tome UI and fun/comprehension playtests
are unexecuted. Windows/macOS, other compilers/ABIs, installed Engine SDK and full
Engine builds are unverified. Words are invented; no actual Elvish grammar or
external language pack is implemented. Private syntax/API choices remain narrow
and reversible. Public documentation generation is outside M1 and is not run.

Next evidence: coordinator reviews this compiler receipt and selects a bounded M2
reference-host task, starting with explicit live selection/recheck, deterministic
simulation advancement and whole-step conflict policy. Ray scope earns only an
independent delivery-edit witness here; its next evidence is an actual contact/ray
eligibility/occlusion refusal in that host. No M2 code begins in this unit. The worker
ends idle; the coordinator publisher owns ordinary feat/magic backup pushes.
