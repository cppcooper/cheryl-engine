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

# MAGIC-M2 — proposed host contract before implementation

Starting clean at 3980904cf9fc03fac913d1fafd56c6f17d677b4e on feat/magic.
Run 20261024T035059Z-885902496b58, same assigned worker/thread; runtime record
confirms configured ultra reasoning and admitted memory scope. Current assignment
splits runtime spoken execution from later item/event/checkpoint work. S5 remains
compilable/inspectable and runtime-refused. No integration/sibling changes.
M1 contracts and intentional v3 provenance above remain evidence; recent compiler
APIs are compatibility constraints, not a new claim about personal API taste.

Proposed private caller, recorded before adding host APIs:

```cpp
auto host = ReferenceHost::create(current_game_catalog, limits);
auto caster = (*host)->add_actor(caster_spec);
auto recipient = (*host)->add_actor(recipient_spec);
auto plan = Magic::compile(edited_verse, compiler_catalog);
auto preview = (*host)->preview(*plan, {*caster, *recipient, std::nullopt});
auto ticket = (*host)->prepare(*plan, {*caster, *recipient, std::nullopt});
auto released = (*host)->release(*ticket); // rechecks live state/schema, then adopts whole step
// For burst: request has a world-mm aim point instead of contact.
auto observed = (*host)->advance_to(100); // explicit ticks, no service or wall clock
```

M2-016 (proposed): finite private host, actor slot+generation identities, immutable
owned reports, one host-incarnation-bound prepared ticket. A new preparation
invalidates the old ticket; successful release/discard consumes it. Ordinary
world changes make a preparation stale; live schema/permission/resource/contact
checks run before revision comparison for precise actionable refusal. Failed
release keeps the token and game state intact; a successful/repeated or foreign
input refuses. Preview issues no token and changes nothing. Host edits are trusted
fixture-owner operations, not security credentials for arbitrary native callers.

M2-017 (proposed): release costs 10 mana per effect, paid once by the caster.
Delayed release reserves one pending slot, captures the accepted world point and
retains the immutable M1 plan handle. Current members are queried on activation;
no future target/status capacity is promised. Cancellation frees the scheduled
slot, with no refund after accepted release. Advance stages the complete finite
step; any due refusal leaves clock, actors, resources and queue unchanged. There
is no automatic retry. Owner must correct the cause or explicitly cancel that
action. Same-tick activations run by increasing accepted action ID, with status
expiry first. This is deterministic reference ownership, not external delivery.

M2-018 (proposed): resolve every binding before writes. Any two writes to the same
actor/health field or actor/speed field in one activation refuse, including across
distinct bindings; binding names and actor storage order do not decide mechanics.
An active slow refuses any replacement/stacking; expiry restores multiplier 1000
and the next activation at that tick may apply a new one. Eligibility is frozen
for the activation before effects; health clamps to [0,max], status applies to
those selected even if simultaneous damage reaches zero. Later queries exclude
dead actors. These are reversible fixture laws, not balance or production combat.

Planned bounds: 16 actor slots, 8 pending casts, 16 active slows (one per actor),
128 selected (binding,actor) pairs and 1024 selector visits per complete step;
16,384 charged work units; one retained prepared plan plus up to eight pending
plans, each <=32,768 M1 logical bytes. World frame G0 only, positions +/-1,000,000
mm, contact centre distance <=50 mm, integer health/mana/base speed <=1,000,000;
clock, action/ticket/revision/generation counters have explicit tightenable caps.
World/scratch/report collections are fixed arrays; plan handles avoid deep copies.
Allocation for host/catalog/report occurs before nonthrowing adoption and may
throw without changing the existing host or ticket. Exact limits and executed
boundary evidence will be recorded after implementation.

## M2 implemented contract and evidence

The private native project now links `magic_reference_host` to the unchanged M1
compiler. `magic_reference_demo` is a small ordinary caller that compiles actual
fixture files, previews/prepares/releases them, advances simulation explicitly,
edits real actor positions and reads actual snapshots. It executes S1, S2, S3,
S4 and the novel mixed-school heal+slow spell. It prints S5's actual immutable
inspection, then verifies `UnsupportedEntry` and unchanged state. There is no
provider/network, random interpretation, Engine/sibling dependency, physics,
background loop, arbitrary callback or external effect path.

M2-016–018 are implemented as proposed above. The API is intentionally private,
with one reference owner and explicit edits. This is a selected engineering
experiment, not evidence of a personal preference inferred from agent code.
Intentional v3 evidence and its confidence/counterevidence remain in the M1 record.
No new personal API inference or sibling inspection is needed for this unit.

### Identity, ownership, authority and adoption

`ActorId` is a host-local slot (0..15) plus nonzero generation. Removal preserves
the slot generation; the next add increments it. Creation chooses the first free
slot; exhaustion of that slot's generation refuses rather than silently finding a
new identity policy. Actor references are logical host-local values, not global
credentials. Add/update/remove/catalog replacement and cancellation are trusted
fixture-owner operations; this library is not a sandbox against arbitrary native
code. Position, health/mana/base speed and permission/eligibility/willingness are
owned finite values. No external actor pointers or live camera references exist.

`Ticket` has private fields: a retained host-incarnation marker plus monotonic
serial. Default/foreign/superseded/consumed tickets refuse. Copying a token does
not create another use; host destruction cannot make its token valid in a new
host. There is at most one prepared record, retaining the plan and value request.
Successful new preparation supersedes the old token. Failed preparation preserves
the prior record (including ticket-exhaustion refusal). Preview issues no token,
changes no world/revision/resource/action and has no release authority. Delayed
preview validates current support and resources without promising future targets,
status capacity or a free queue slot.

Every release revalidates caster generation/aliveness, can_cast permission,
contact/aim shape, current schemas and available release mana before comparing the
prepared world revision. Any ordinary owner edit, successful release/cancellation
or time advancement changes revision and makes old preparations stale. Live
permission/resource/contact/schema failures have specific diagnostics before the
stale check. Failed release preserves the prepared input and complete world;
successful release/discard consumes it. Irrevocably stale input needs a fresh
prepare. Discard changes only prepared authority, with no gameplay charge.

Plans are the actual `shared_ptr<const Magic::Plan>` artifacts from M1. M2 copies
no source, compiler tree, key collection or Common text into runtime programs.
Prepared/pending records retain handles; bounds check the M1 logical retained-byte
metric before taking a runtime reference. Removing the final queue reference can
destroy the original plan after all source/catalog/compiler owners are gone.
Observations own fixed value snapshots and action events, and retain no plan or
host pointers. Const observation handles remain usable after later steps and
host/plan destruction. Snapshots returned by value can be edited without changing
the host. Callers own any observations/tickets they retain; the host does not keep
an unbounded output history.

Current schema validation compares **both exact symbolic key and primitive**, then
checks positive quantity and duration against the current host maximum, primitive
shape and supported medium/delivery/bundle flags. An M1 Heal plan with a foreign
key cannot dispatch by enum. A known key whose primitive changes also refuses.
Current hot-replaced quantity, duration or capability limits are rechecked before
release and again at delayed activation. Existing accepted active slows retain
their settled expiry; schema replacement is not retroactive rollback.

All gameplay mutations use finite scratch state. Release stages complete writes,
resource debit, accepted action ID and queue insertion. Advance stages every due
action and expiry up to the requested tick. An immutable owned Observation with
before/after world and accepted action events allocates **before** nonthrowing
state adoption; static assertions cover state/result moves. Actor edits use fixed
value copies; catalog replacement validates and copies bounded strings before
nonthrowing catalog/world adoption. Standard allocation exceptions propagate with
the old host/prepared input intact. Allocation-failure injection is not executed;
this guarantee is established by source ordering and noexcept assertions, not an
OOM experiment. No arbitrary external callback transaction is claimed.

### Time, resource and overlap laws

The caster pays exactly **10 mana per effect on accepted release**: 10 for S1/S3,
20 for S2/S4/mixed healing+slow. Failed prepare/release pays nothing. Delayed
release reserves one queue slot and accepts the prepared candidate point as a
fixed world-mm G0 aim. Changing the request, camera authoring context or caster
position afterward cannot move this point or the already normalized radius.
Activation queries current eligible, living actor centres, including the exact
radius boundary, and checks current caster lifetime/permission/schema. No second
mana debit or funds reservation occurs; a cast paid from exactly 20 mana executes
with zero mana remaining.

Contact is immediate and singular: the explicit actor must still be living,
eligible, willing and within inclusive **50 mm** centre distance of the caster.
There is no fallback. Burst supports the M1 inclusive disk query in G0; actor
willingness is a contact predicate, while disk membership uses the host's eligible
flag. The reference policy has no faction, line-of-sight, knowledge, resistance,
physical contact manifold or production combat permission system.

S2/S4 due time is release+**100 ticks**, each 10 ms. Slow is active from activation
through the tick before activation+**200 ticks**: [100,300). S3 uses [0,200).
`advance_to` accepts nondecreasing bounded simulation ticks; no wall time is read.
It visits finite event boundaries, expires statuses first, then activates casts
by ascending accepted action ID at each due tick. Distinct accepted actions use
this declared chronological/ID order; an individual together bundle is never
converted into sequential authored `then`.

Every binding resolves from one pre-activation snapshot before writes. Selected
(binding,actor) pairs retain separate scope even when both names select the same
actor. Two writes to that actor's health field (heal/heal, fire/fire or heal/fire)
or speed field in one activation refuse as `Overlap`, independent of binding name,
effect order and actor storage order. A speed write encountering an existing slow
refuses the entire activation as `ActiveSlow`; there is no stacking, replacement
or refresh. Expiry occurs before a new activation at the same tick, allowing a new
independent status then. Base speed stays separate and the active multiplier is
exact permille; effective speed is represented in thousandths of mm/s with no
rounding or movement integration.

Health clamps to [0,max_health]. Eligibility is fixed before the whole activation;
a selected actor can reach zero health from fire and still receive that same
bundle's finite slow. Future queries exclude zero-health actors. These are explicit
fixture laws, not implied death/resurrection or balance mechanics.

Any due failure rejects the **complete advance**: original clock, active statuses,
health/resources, action counter and every pending slot survive, including earlier
successful scratch actions in that attempted step. Failure records the attempted
event tick and accepted action context. A failed release allocates no action ID
(error action=0); successful logical IDs are host-local and never reused. Nothing
automatically retries a refusal. The owner must explicitly correct the cause or
cancel the pending action. Cancellation frees its slot/plan handle and does not
refund accepted release mana; a repeated cancellation refuses. A poisoned queue
can therefore block advancement until the owner acts. This tradeoff is deliberate.
There are no ambiguous partial external effects because this host has none.

### Independent literal runtime baseline and actual caller

Contract checks compare full before/release/onset/expiry snapshots against authored
values. Only the queued plan's byte accounting is taken from its compiler metric;
world mechanics are independent literals. The common starting actors are caster
C=(225,0), health100/mana100/ineligible/can_cast; T=(250,0), health50/eligible;
O=(251,0), health80/eligible. Maximum health100, base speed1000 mm/s. G0 aim=(0,0).

| Fixture | Release state | Onset state | Expiry boundary |
| --- | --- | --- | --- |
| S1 | tick0 C mana90, T health62; no queue/status | immediate | no status |
| S2 and S4 | tick0 C mana80, T health50, due100; tick99 unchanged health/speed | tick100 T health42, factor750 until300; O health80 unaffected | tick299 factor750; tick300 factor1000, T health42 |
| S3 | tick0 C mana90, T health50, factor750 until200; O unaffected | immediate | tick200 factor1000, T health50 |
| Novel heal+slow | tick0 C mana80, due100 | tick100 T health62, factor750 until300 | ordinary shared status expiry |
| S5 | plan inspection remains exact | runtime UnsupportedEntry, no mutation | no installed charge/event |

The actual `magic_reference_demo` additionally moves T to (1000,1000) after release
and moves O to (150,200), exactly 250 mm from the captured aim. At tick100 T remains
health50/factor1000; O changes health80->72 for S2/S4 or80->92 for the novel healing
bundle and gets factor750 until300. Output is read from adopted snapshots and
asserted, not a transcript of assumed execution. An independent caller scenario
also changes the camera/scale context and request after preparation; the accepted
point and 250 mm world radius remain fixed.

### Actual resource and arithmetic model

All HostLimits can tighten fixed ceilings, including zero; expanding any of seven
size quotas or five uint64 counter horizons refuses. Identity/time counters check
exhaustion before addition. Positions are validated before int64 subtraction;
squared unsigned distance multiplication/addition is checked before the inclusive
comparison. Health addition uses a bounded uint64 intermediate; no allocation size
comes from an authored coordinate, duration or quantity.

| Resource | Bound and evidence |
| --- | --- |
| Actors | 16 fixed slots; lower quota enforced, including zero. Each generation <=1,000,000. First-free exhausted generation refuses. |
| Pending casts | 8 fixed slots. Eight accepted releases then a ninth refusal preserves queue/funds; cancellation permits a fresh monotonic action in the freed slot. Pending=0 still allows immediate contact. |
| Active statuses | 16 fixed actor-local slots, one slow per actor. Exact 16 accepted;15/0 refuse. Capacity is checked at activation, with no future reservation promise. |
| Targets | <=128 (binding,actor) selections across a complete operation. Eight casts selecting16 actors exactly reach 128;127 refuses the entire advance. |
| Selector work | <=1024 visited slots across the complete operation, including empty slots. Eight casts x8 bindings x16 slots exactly reach1024 with only 64 selected pairs;1023 refuses without adoption. |
| Complete work | <=16,384 charged units. Fixed80 setup/copy/report units plus variable schema/name checks, selector visits, selected pairs, field scans and event scans. This is a deterministic operation model with fixed-width helper costs, not CPU cycles or a timing guarantee. Exact S2 activation work308 accepts,307 refuses and preserves paid queue/world. |
| Runtime plan retention | One prepared plus up to8 pending handles, each M1 logical bytes <=32,768, at most288 KiB distinct logical plan payload charged conservatively; no deep runtime copies. Exact per-plan metric accepts and one less refuses. M1's metric excludes unused capacity/control blocks/allocator overhead. |
| Clock/revision | Clock0..1,000,000,000 ticks; revision0..1,000,000,000. Nondecreasing advance; expiry/due arithmetic checked. A fire-only cast due exactly at the horizon succeeds without adding onset twice. |
| Action/ticket IDs | Monotonic host-local values1..1,000,000; zero not an accepted identity. Tight horizons prove action/ticket/revision exhaustion with no debit/queue mutation or ID reuse. |
| Actor scalar fields | Coordinates +/-1,000,000 mm; positive max health <=1,000,000; health within max; mana/base mm/s speed0..1,000,000. Contact <=50 mm; compiler's radius <=1,000,000 mm. |
| Retained state/outputs | Fixed arrays, bounded catalog of at most3 schemas and64-byte keys; one host marker. This ABI reports sizeof host 1872, Snapshot 1496, Observation 3216 bytes, excluding separately allocated catalog/marker/control blocks and retained plans. Each report allocates one bounded object/control block before adoption; caller-retained reports have no host-owned history cap. No exact heap/RSS guarantee is claimed. |

### Commands, results and saved logs

Current build reuse is inspected before changes: `projects/magic/build`, Debug,
GNU16.2.1, C++23, MAGIC_SANITIZERS=ON. All configure/build/native checks use the
workspace wrapper, one job, clone-local incremental output; complete stdout/stderr,
exact command and immediate `exit_status` are saved. No native lock cancellation,
resource refusal, allowance/auth workaround, dependency retrieval/install or sibling
cache/process/ref write occurs. Existing LSan tracing limitation is preserved;
ASAN_OPTIONS=detect_leaks=0 is command-local, UBSAN_OPTIONS=halt_on_error=1 remains.
No leak acceptance is inferred. Existing compiler/M1 source, fixtures, checks and
prior logs remain unchanged.

| Actual wrapper command (from clone root) | Complete evidence, result |
| --- | --- |
| `/mnt/projects/cheryl-features/scripts/build -- cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug` | `magic/logs/M2/configure-01.log`, exit0. |
| `/mnt/projects/cheryl-features/scripts/build -- cmake --build projects/magic/build --parallel 1` | `magic/logs/M2/build-01.log`, `build-02.log`, `build-03.log`, all exit0. Later builds add the three activation checks, header contract/include clarification and actual S5 inspection output. |
| `/mnt/projects/cheryl-features/scripts/build -- cmake --build projects/magic/build --target magic_host_checks --parallel 1` | `magic/logs/M2/build-04.log`, exit0; strengthens the overlap witness to create the same logical actors in both actual insertion orders. |
| `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /mnt/projects/cheryl-features/scripts/build -- ctest --test-dir projects/magic/build --verbose --parallel 1` | `magic/logs/M2/accumulated-01.log`, exit0, **11/11 entries**. Runs the entire prior 9-entry M1 suite exactly once plus initial 34 host cases and 6 reference caller scenarios. |
| Same environment and wrapper CTest command with `-R '^magic_(host_contracts\|reference_caller)$'` | `magic/logs/M2/affected-01.log`, exit0, **2/2 affected entries**. Final **37 host cases** and 6 caller scenarios after source/test changes; no repeat of unaffected M1 entries. The saved log contains the exact shell-quoted regex (the table escapes its pipe for Markdown). |

Same environment/wrapper CTest command with `-R '^magic_host_contracts$'` is
saved in `magic/logs/M2/affected-02.log`: exit0, **1/1 affected entry**, all 37 host
cases pass after the insertion-order witness is strengthened. Production host code
and the already-checked reference caller do not change in this final test edit.

Final unique suite remains 11 CTest names: prior 9 M1 names (35 compiler cases,
4 ordinary callers, 6 file fixtures, 9 CLI processes), one host entry with 37 final
cases, and one reference caller entry with 6 scenarios (five spoken executions,
one S5 inspection/refusal). The focused runs recheck existing names (two entries, then one); they
do not increase the unique count. All executed builds and native checks pass; no sanitizer
findings appear. Format, evidence/source hashes and staged review are saved in
`magic/logs/M2/static-review.log` and `staged-review.log` during finalization.

Executed host coverage includes independent full literals, amount/clamp/eligibility,
contact50, current moved/departed/replaced targets, accepted aim/context lifetime,
plan lifetime, immutable old observations, forged/foreign/superseded/consumed and
stale authorization, live permission/willingness/mana/contact, key/primitive/hot
schema/capability refusals, active-status rules at prepare and activation,
heal/fire and speed overlap in both actor/effect orders, complete-step rollback,
explicit cancellation, empty selections, actor/queue/status/selector/target/work/
plan/time/ID horizons, invalid owner inputs and unsupported ray/engraved/frame.

## M2 handoff and remaining boundaries

M2 is ready for coordinator review as one finite spoken reference host. S1–S4 and
novel healing+slow produce actual reference-state changes. M1 accepted compiler
coverage remains intact. This unit does not implement item charges, authenticated
trigger dispatch, replay/dedupe, checkpoint restoration, durable saves or external
exactly-once delivery. S5/ray/item-event are runtime-refused; only release-mode
spoken contact and immediate/delayed G0 disks execute. No general VM, physics,
Engine combat integration, line-of-sight, graphical spellbook, learner progression,
production mana/balance system or fun/comprehension playtest is established.
GNU/Linux Debug ASan/UBSan is verified; leak checking, allocation fault injection,
other platforms/ABIs/toolchains and Engine SDK integration remain unverified.

The finite transactional scratch owner is the scope growth that strengthens this
proof: it makes live overlap, failed scheduling and resource accounting observable
without callback rollback claims. Its fixed sizes and private API keep it
reversible. Next evidence is coordinator review and a separately assigned bounded
item/event/charge/checkpoint unit if selected; that unit must define event authority,
deduplication retention and coherent armed/spent restore without claiming durable
production delivery. No such work begins here. Worker ends idle after its local
checkpoint; coordinator publisher handles ordinary feat/magic backup.

## MAGIC-M3 — proposed caller and decisions before API growth

Accepted predecessor inspected: clean feat/magic at 8b6dcad36002eaaf9007b94b712bb2cf2288a64a; M1/M2 receipts, actual compiler/host and accumulated/affected logs agree with the coordinator's counts. M2 remains spoken-only through prepare/release; this unit adds a distinct native-owner item entry. No intentional v3 taste inference is added: earlier intentional author/commit provenance remains the evidence; M1/M2 interfaces establish compatibility only. Runner admission records configured ultra for this exact task.

Proposed small caller (not yet executed):

```cpp
plan = compile(healing_engraving, schema);             // actual owned M1 plan
host = ReferenceHost::create(schema, limits);
item = host->install(plan, funding_actor);            // trusted fixture owner; prepay
armed = host->checkpoint();
host.reset();                                       // no compiler/catalog/source lifetime dependency
restored = ReferenceHost::create(schema, limits);
restored->restore(armed);                            // validates; fresh live incarnation
event = restored->admit_touch(17, item, toucher);     // owner admits a logical native fact
result = restored->touch(event);                     // shared payload, heal + charge + receipt together
spent = restored->checkpoint();
restored->restore(spent);
restored->touch(restored->admit_touch(18, item, toucher)); // explicit Spent refusal
```

**M3-019 (selected, reversible private contract).** Installation is an explicit trusted native-owner operation, using a living can_cast funding actor's current mana and current key+primitive/quantity/capability schema. It accepts only immediate one-charge item-touch plans with event-toucher bindings, pays 10 mana/effect at installation, and retains the actual immutable plan. A successful touch spends the prepaid charge; failed installation/application, disable/remove, and cancellation never refund. Items have first-free slot/generation identities, enabled state, and a per-installation plan; spent stays visible and inert. No physical item/contact simulation is claimed: owner admission asserts the touch fact. Spoken/item payload resolution and overlap/status/write logic are shared, with an explicit event recipient rather than a fake caster. Confidence high in ownership/refusal, medium in fixture pricing/API.

**M3-020 (selected).** At most eight items and sixteen lifetime touch receipts, caller-tightenable. Native-owner admit_touch accepts a nonzero bounded logical fact ID plus item/toucher references, checks duplicates, and returns an opaque incarnation-bound capability with a checked monotonic nonce. An ID alone cannot apply effects. Each admitted fact remains in the ledger forever within this timeline, whether outstanding or consumed; no arbitrary eviction or TTL. Failed application preserves the entire world/items/ledger and leaves admission usable after explicit correction; successful application consumes it atomically with payload/charge. Default, foreign, consumed and removed/replaced-item inputs refuse. Copies share one use. Restore revokes all old live inputs; the trusted owner may resume an outstanding logical fact with resume_touch, which issues a capability only for that existing unconsumed ledger entry. Duplicate admission always refuses. Fact/nonce horizons and fixed ledger capacity refuse explicitly without wrap. This is native owner authority, not network authentication. Confidence high; lifetime ledger exhaustion is an intentional finite limit.

**M3-021 (selected).** Checkpoint is a separate opaque immutable owned value, never Snapshot. It retains complete finite state, actual pending/item plan handles, schema/limit identity, ledger and issuance counters; it excludes live prepared casting and event capabilities. Capture and restore validate bounded state, canonical slots/generations, references, times/statuses, exact schema/limit compatibility and retained payloads before allocation/adoption. Restore into an existing compatible owner replaces the whole timeline with a fresh incarnation, clearing prepared input; failed restore preserves that owner and previous outputs. Whole-world rewind intentionally restores charges/health/clock/ledger to that earlier world; duplicate delivery is prevented within each resulting timeline, not across deliberate rewinds. No disk/process/crash/external exactly-once guarantee. Confidence high in narrow in-memory design; no allocator fault injection promised.

Plan limit accounts per handle payload (existing M2), fixed arrays bound owner/checkpoint metadata, and checkpoint validation meters complete scans/key comparisons. Caller-retained immutable checkpoints/observations are outside host history quotas. Allocation exceptions propagate before adoption; no arbitrary callback participates. Pending actions may retain currently departed actors as cancellation-only references, but restore will require live referenced actors so such a checkpoint explicitly refuses until corrected/cancelled; no implicit repair. Source/schema edits can likewise require correction/cancellation before capture. These narrow choices strengthen coherent continuation without building a general event bus or save format. Next evidence: literal armed/spent worlds, real owner destruction/restore and delayed boundary continuation, then focused refusal/budget cases plus accumulated regression once.

## MAGIC-M3 implemented contracts and evidence boundary

M3-019–021 are implemented with the proposed ownership boundaries. New ordinary caller is `projects/magic/src/engraving-demo.cpp`, separate from test helpers. It compiles an invented Mend healing clause, prepays an item, destroys its owner/compiler input, restores armed, applies an admitted native touch (+12hp, charge1->0), destroys/restores spent and observes SpentItem with no extra healing. Its delayed example retains an actual compiled plan, nonzero world aim(1000,-500), inclusive radius250mm, due100 and slow expiry300, destroying/restoring both pending and active owners. It advances explicitly at99/100/299/300 and verifies no second activation. These are executed native assertions when the saved caller log passes; no callback or source regeneration supplies continuation.

`Reference::Snapshot` remains the same observational type and cannot convert to Checkpoint. New ItemSnapshot is also observational and exposes item/charge/receipt facts, never a way to forge event authority. Checkpoint has only immutable opaque shared data, no mutable state constructor. Each capture retains up to eight pending plus eight inscription plan handles and one bounded catalog copy, counters/limits and fixed arrays. No text or plan deep copy occurs at runtime. A host may additionally retain one prepared plan: at most17 handle payloads ×32768 logical bytes =557056 logical payload bytes before handle sharing; each checkpoint retains at most16 ×32768 =524288. Allocator/control-block overhead and caller-retained checkpoint/report history are outside quotas. All are finite, not exact RSS promises. State copy overhead is fixed in the operation model; capture/restore additionally charge220 units for bounded schema metadata/keys before validation/copy, and charge every state scan, ledger uniqueness comparison and retained plan check.

Installation authority is the trusted single-thread native owner API plus a living can_cast funder with sufficient mana. It does not require a spoken target or use prepare/release. The charge is prepaid (10/effect) at successful installation, even if the eligible recipient is already at maximum health; a later successful clamped activation still consumes its single charge. The funder may subsequently depart; the item payload has no fake caster or ongoing mana dependency. Trusted admission asserts contact; no item position/physics or network authentication is modeled. Enabled=false refuses application without consuming admission; reenable can make that same unconsumed capability usable. Removal retains the generation counter and ledger, frees only the inscription handle, and never refunds. First-free exhausted generation refuses rather than skipping it.

Fact IDs are supplied only at the trusted owner admission boundary (range1..max_fact_id). They are not arguments to touch; default/copied observations/IDs cannot create TouchEvent. Admission assigns nonce1..max_event_nonce and appends one of16 fixed lifetime receipts (outstanding and consumed both count). There is no time TTL or eviction; issuance horizon is count/ID bounded. Admission itself changes ledger/revision but does not heal/spend. Failed application preserves that admitted state, charge and ledger bit; no automatic retry. Successful application resolves all bindings before writes and stages the entire state and owned ItemObservation before nonthrowing adoption. Copying or owner resume of a capability cannot produce a second success. resume_touch is an explicit trusted reauthorization of an existing unconsumed fact, not arbitrary raw-ID execution; old live inputs remain foreign after restore.

Checkpoint capture and restore both validate: clock/revision/action/ticket/event horizons; canonical slots, scalar ranges and generation counts; status bounds and expiry; pending action uniqueness/references/due ticks/aim, real retained plan and current spoken capability/schema; item charge/prepayment/plan identity and current engraved capability/schema; ledger nonce order, unique fact IDs, bounded historical references. Historical departed/replaced item/actor references remain valid replay tombstones, not usable live targets. Pending work with departed/unpermitted caster or a changed unsupported schema refuses capture until owner correction/cancel; pending effect durations beyond max_tick also refuse. Existing active slow is committed state that only expires, not an effect redispatch. Exact catalog ordering/keys/schemas/flags and all HostLimits must match the destination. Restore stages a new owner marker and state before adoption, restores logical counters but clears prepared input, and invalidates every old casting/event capability. Failure preserves current host, checkpoint and old outputs. Checkpoint does not retain live tickets/owner markers. Allocation exceptions still propagate before adoption; arbitrary failure injection is unexecuted.

Whole-world restoration is an intentional rewind: restoring an armed checkpoint can legitimately admit the same fact again in a new timeline after health/charge/ledger have also rewound. Restoring spent preserves its receipt and inert charge. This proves no duplicate inside one restored timeline, not delivery across intentional rewinds, processes, disks, crashes or external systems. Catalog/limit migration and cyclic/general triggers are deliberately unsupported.

### M3 actual checks and handoff

Configured/run in the existing clone-local `projects/magic/build` Debug C++23 ASan+UBSan tree, GNU16 toolchain, warnings as errors, one job. Every configure/build/native CTest command goes through `/mnt/projects/cheryl-features/scripts/build --`. Full stdout/stderr, exact command and immediate exit are saved under `feature-development/magic/logs/M3/`; all seven native invocations exit0. Build02 waits normally for the shared lock in its original tool session; no cancel/relaunch or resource-refusal retry. No dependencies, pins, siblings, archives, remotes or shared caches change. Existing no-swap/adaptive/floor policies apply. Worker run20261024T051119Z-e3455591c7fa, configured ultra, accepted base8b6dcad.

| Log | Actual command after wrapper (environment on native CTest below) | Result |
| --- | --- | --- |
| configure-01.log | `cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug -DMAGIC_SANITIZERS=ON` | exit0, existing incremental configuration |
| build-01.log | `cmake --build projects/magic/build --parallel 1` | exit0, all nine executable/library targets built (seven executables, two libraries) |
| accumulated-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1` | exit0,13/13 entries once: all9 M1 entries,2 M2 entries,2 M3 entries; initial29 M3 contract cases and3 new caller scenarios |
| build-02.log | `cmake --build projects/magic/build --target magic_engraving_checks magic_engraving_demo --parallel 1` | exit0 after checkpoint-only horizon/copy-meter review changes and combined-state coverage |
| affected-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1 -R '^magic_engraving_(contracts\|caller)$'` (actual regex has unescaped `|`, see saved command) | exit0,2/2,32 M3 cases and3 callers |
| build-03.log | same two affected build targets | exit0; caller/test-only nonzero-aim and restored-exhaustion witnesses |
| affected-02.log | same two affected CTest names | exit0,2/2, final34 M3 cases and3 callers; no sanitizer findings |

All CTest commands set only command-local `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`, preserving address/undefined checks and the known tracing-related leak-check gap. No new full regression rerun follows passing accumulated checks: later production changes are inside new checkpoint validation only, and final changes affect only new tests/caller. M1 retains35 compiler cases,4 ordinary callers,6 file fixtures and9 CLI process scenarios; M2 retains37 spoken-host cases and6 reference callers (five spoken executions and one engraved spoken-entry refusal). These counts are not extra CTest entries. New total is13 unique CTest entries, not17 due to focused reruns.

Independent literal before/admission/touch/restored armed/spent worlds assert health50->62, prepaid mana100->90, charge1->0 and receipt false->true. Tests also cover exact target identity/current willingness/eligibility/liveness; live unknown/changed key, primitive, quantity and entry capabilities; default/foreign/copied/repeated capabilities; disabled/spent/removal/reuse; actor/item generation references; exact8 items,16 retained receipts,1/0 nonce horizon and bounded fact IDs; exact/insufficient10/9 funding; per-plan bytes exact/one-short; touch exact168 work vs167, selected/selector1 vs0; capture/restore fixed base-state exact348 work vs347 capture refusal; revision/time horizons; coherent combined items/receipts/pending/slow; owner/source/catalog destruction; old report immutability; fresh restore authority and explicit outstanding resume; incompatible/default checkpoint refusal and continued prior casting/checkpoint usability. Nonzero accepted aim(1000,-500) survives caster movement, owner destruction and restore, with inclusive boundary(1250,-500) included. Restored nonce/generation exhaustion never resets or wraps. Final native assertions are34 cases; individual loop variations are not counted again.

M2's actual full Snapshot literals and overlap-order checks remain unchanged. M3 adds ItemSnapshot literals; source-dependent `plan_bytes` comes from the already validated input plan metric, while health/resource/charge/timing/identity values are independent literals. Opaque Checkpoint has no caller-editable state constructor; invalid-state checks are exercised through invalid capture and incompatible/default restore, not a forged deserialization API. The restored active status is data with a future expiry; it never redispatches old effects.

Static/staged review is saved separately. No native failures occurred. Pending boundaries remain: no disk codec, process restart, crash/power-loss safety, remote/event authentication, external exactly-once guarantee, automatic rearming, general trigger graph/bus, item physics, ray/non-G0 runtime, production Engine integration, balance/fun/graphical/learner acceptance, LSan under tracing, Windows/other toolchains/ABIs, allocator fault injection or exact heap accounting. Native-owner allocation exceptions are source-reviewed for before-adoption preservation, not OOM-tested. Catalog/limits must match exactly on restore; no migration is selected. Checkpoints/reports retained by callers can accumulate outside host quotas.

Next action: finish this single coherent M3 checkpoint and remain idle. The finite controller alone may admit pinned MAGIC-M4 after this committed passing receipt, clean/drained predecessor and saved evidence gate; M4 must inspect real M3 source/receipt/logs before depending on it. No M4/M5 work or scheduling starts here. Publisher owns ordinary feat/magic backup transport.

## MAGIC-M4 — proposed caller and selected contracts before APIs

Predecessor gate inspected at clean9321bedd7d7543c1b7e96b1459d02122edfa2abb: committed M3 receipt matches feature-round-20261024-0500/MAGIC-M3, ready/true, and all seven committed nonempty native logs end exit0. Actual source retains shared key+primitive validation/payload writes, opaque item authority, non-evicting receipt ledger and distinct coherent checkpoint with fresh incarnation. Saved evidence confirms13 accumulated entries, final34 M3 cases/3 callers plus retained M1/M2 counts. No prerequisite contradiction. Runner20261024T052652Z-325fb79d80d5 records configured ultra. Existing intentional v3 provenance remains above; recent APIs establish compatibility only, with no new taste inference.

Proposed ordinary caller (names provisional, not yet executed):

```cpp
book = Spellbook::create(limits);
book.add("healing", "A small restoration");
book.edit("healing", mend_contact, notes, catalog, learner); // accepted revision1
lesson = make_lesson(bilingual_fire_slow, exact_gloss, registered_cinder_slow_rule,
                     changed_practice_example, catalog, limits);
book.edit("healing", newly_composed_mend_heal_plus_cinder_slow, notes, catalog, learner);
// Compiler recognizes it, learner lacks cinder.slow@1; revision1 survives.
learner.learn(lesson); // teaches just that production, idempotent; no host token
new_entry = book.edit("healing", newly_composed_mend_heal_plus_cinder_slow, notes, catalog, learner);
print(new_entry.accepted.common());
ticket = host.prepare(new_entry.accepted.plan(), ordinary_request);
result = host.release(ticket); host.advance_to(100);
book.edit("healing", malformed_draft, notes, catalog, learner); // revision2 remains usable
```

**M4-022 (selected, reversible).** Add exact bounded compiler metadata for eight registered rules: three Common effect concepts, explicit target bridge, Mend heal/slow and Cinder fire/slow. The actual checked production records its rule per clause and an owned value mask in Plan; no text heuristics. School clauses also require the corresponding Common effect concept plus bridge. The fixture always recognizes outer Common constructors; learned syntax is a separate consumer policy. Common can replace unknown school phrasing only when its operation concept/bridge is learned, and the host still checks current key/schema/permissions. Medium confidence in private registry shape, high in separating recognition/knowledge/authority.

**M4-023 (selected).** An eight-slot private book owns identities/titles and immutable accepted revision handles, plus one current owned Attempt per slot. Accepted source/Common are owned by the retained actual Plan, annotations by the entry. Failed syntax, unsupported capability, unknown knowledge, revision/work/storage/text edits keep accepted source/plan/revision and record a bounded repairable draft/diagnostic; oversized drafts keep only the allowed prefix and original byte count. Successful edits increment a checked per-entry revision; no identity reuse/removal/history API. Old handles/queued/item/checkpoint plans remain unchanged. Caller-retained history is outside book quotas. High confidence in ownership; medium in reversible API.

**M4-024 (selected).** One immutable tome lesson owns a worked bilingual source/plan, exact canonical Common gloss, a changed practice source/plan/gloss, annotation and compiler-derived aligned clause copies. A named registered school production must occur in both actual examples; contradictory gloss, unknown rule/identity or unchanged practice meaning refuses before learning. Knowledge is an owned version1 eight-bit rule record with checked revision; learning requires known operation concept/bridge and idempotently sets only the taught bit. No reward ledger, provider or host authority is added. High confidence in deterministic checks; human teaching/enjoyment are hypotheses.

Limits selected before input: eight book entries, one lesson per immutable tome object, eight registered rules and at most four aligned effect clauses. IDs are1..32 ASCII identifier bytes, title<=64 and annotation<=256; each authored/gloss/draft text<=8192 bytes. Learning limits may tighten entries/text/revision, work<=262144 and retained logical bytes<=524288. Reserve12288 work per operation and12288 retained bytes per configured book slot (one lesson envelope for tomes) for bounded draft/diagnostic/metadata copies, including failure publication; work cannot tighten below that reserve. Further compile work is admitted through M1's remaining per-call work budget (<=65536), with actual successful metrics charged; retained actual plan payloads and aligned text copies are added with checked arithmetic. Fixed envelopes conservatively cover strings/metadata, not allocator overhead/RSS. Limits/refusals happen before variable allocations; allocation exceptions propagate before pointer adoption. Failure publication uses its reserved bounded envelope even when the payload budget is exhausted. This choice keeps over-budget attempts inspectable without unbounded failure history. Next evidence: exact literal meanings/Common, actual lesson transfer/cast, all selected budget boundaries, ownership and predecessor regressions.

M4-024 refinement before checks: the learner retains at most four immutable lesson handles, one per teachable school production. It refuses reuse of a studied identity with different rule/annotation/source/practice, while independently owned equivalent definitions are idempotent. This small addition proves duplicate identity handling without a general lesson catalog or reward system. Four handle assignments do not deep-copy lesson text; each lesson payload remains bounded by524288 logical bytes, at most2MiB of retained lesson models per learner before sharing. Learn itself has fixed eight-bit/four-receipt work (<=four32-byte identity checks and two8192-byte definition comparisons plus bounded annotation), no variable allocation/callback. Missing concept/bridge or revision exhaustion preserves knowledge. Book compilation admits the replacement's remaining plan-byte quota through M1 before allocation; temporary staging may coexist with the prior accepted plan. Tome retains at most two plan payloads plus aligned clause copies, with at most two additional bounded32768-byte Common-provenance plans during construction. Fixed object arrays/allocator overhead are separately finite, outside the logical payload model; zero entry payload quota does not claim zero object RSS.

## MAGIC-M4 implemented contracts and exact proof

M4-022–024 are implemented. The small native caller is `projects/magic/src/learning-demo.cpp`, independent of test helpers and library internals. Authored originals and independent canonical Common readings are six files in `projects/magic/fixtures/m4/`. The Cinder/Common worked example is fire8 plus slow750 for200 ticks, radius250mm, onset100. The practice transformation changes fire to Mend healing6 and slow to500 for100 ticks, onset50. The caller edits an existing usable Mend12 contact spell into a different combination: Mend healing9 plus Cinder slow600 for300 ticks, radius300mm, onset50. This actual new text uses existing primitive/grammar constructors; the learning library contains no whole-spell dispatch. It differs from both the tome practice and earlier M1–M3 heal/slow examples.

The caller's initial learner recognizes the compiled new plan but lacks exactly `cinder.slow@1`. It owns that failed draft while accepted healing revision1 survives. The lesson prints exact aligned clauses: `on near bridge Cinder@1 { 2 s 750 permille neth } ;` equals `on near bridge Common@1 { multiply speed by 750 permille for 2000 ms } ;`. Both language name and Cinder/Mend tokens are explicitly labelled invented; Common is controlled syntax. Learning the duration-first production is idempotent and yields only that bit. The new edit accepts revision2, prints its exact Common, and still gets SchemaMismatch when the live host's slow max_quantity is599. After restoring supported schema, a malformed sequential edit reports owned UnsupportedComposition at byte285 and leaves revision2 usable. Ordinary host prepare/release pays20 mana, queues due50, observes hp50 at49 and hp59/speed600 at50, and expires speed at350 without another heal. Initial healing handle stays revision1/12hp. This demonstrates fixture behavior, not a human lesson outcome.

Compiler metadata comes from checked Authored language/primitive roles during lowering. It is a fixed owned RuleMask and one Rule enum per existing bounded Provenance record, with sizeof-based retention and one work charge/effect. No source-string search infers learned requirements. Common@1 operations double as operation-concept requirements; all plans also require the explicit bridge. Recognized school productions additionally require their own bits. Outer Common composition/geometry/entry constructors are assumed available to this fixture learner, not individually taught. Unknown school syntax may therefore be replaced with known Common; missing slow/heal/fire concept still prevents availability in the book, and current ReferenceHost capabilities/permissions/resources still decide casting. `magic_learning` links only the compiler; callers explicitly obtain reference-host authority separately. Knowledge is consumer policy, not a sandbox against arbitrary trusted native owner calls.

Entry identity/title/annotation are owned; source and Common are exposed through the retained immutable actual Plan. Accepted revision starts1; checked successful edits increment, failed attempts consume no revision. Each resolved-ID edit returns an immutable Attempt holding the current accepted handle plus either accepted source or a bounded owned failed draft, submitted byte count, truncation flag and structured Problem. Compiler failures retain owned spans/excerpt/repair text. Unknown knowledge retains an exact missing-rule mask. Oversized text keeps at most the configured source prefix; oversized annotations are refused and do not replace accepted annotations. No full unbounded rejected input/history is retained. Allocation exceptions propagate before accepted/draft pointer adoption, preserving prior outputs; this is source-reviewed, without fault injection.

One Lesson owns ID/annotation, actual worked and practice Plan handles and up to four aligned original/Common clause copies. Exact gloss equality and reparse typed meaning, occurrence of the taught registered school rule in both examples, and changed practice meaning validate mechanical teaching data. Annotation is non-executable descriptive text, not authority or an interpreted rule. Unknown version/operation concept as taught school rule, malformed input, contradictory gloss, missing rule or unchanged practice refuse. All fixed grammar names/revisions remain private and reversible; no external language-pack format is introduced.

Learner version1 holds eight known bits/revision and at most four immutable studied Lesson handles. Same studied ID with equivalent source/practice/annotation/rule AND typed meaning is idempotent; inconsistent reuse refuses DuplicateIdentity before any state change, including identical prose compiled with different symbolic keys. Already-known productions add no revision/reward/extra receipt. Prerequisite is the learned corresponding Common operation concept plus target bridge. Each new school rule advances the bounded knowledge revision once; no lesson can unlock another operation by prose or mint Ticket/TouchEvent. Copying book/knowledge/tome/checkpoint data supplies no live authority. Four retained lesson handles avoid deep definition copies; fixed bounded comparisons remain below32768 logical character/field units per learn even when typed meaning is checked.

Budget model is intentionally conservative. Book reserves12288 logical bytes for each configured slot's identity/title/accepted metadata/current failure envelope, plus current accepted Plan metrics; maximum8 slots and eight32768-byte plan payloads. One staged replacement Plan may coexist with the prior accepted plan; M1 admits its allowed replacement payload before allocation. At creation, checked division rejects an insufficient slot envelope before multiplication. Every later logical addition/subtraction is checked before the corresponding variable copy/adoption. Tome reserves12288 plus two actual plan metrics and aligned text bytes; up to two additional temporary Common-provenance plans are each bounded32768. Work reserves12288 for bounded failure/metadata copies and fixed scans, then charges actual successful M1 metrics, canonical comparisons, metadata and aligned copies. M1 compile work admission is min(65536,remaining); failed compilation exposes no exact work metric, so the whole admitted compile work is conservatively charged. These are logical payload/work models, not CPU cycles or exact heap/RSS. Fixed object arrays/control blocks/allocator bookkeeping and caller-retained immutable history are outside variable payload quotas but have finite per-owner ceilings. No new caches/dependency pins or siblings change.

Current source review adds explicit duplicate-identity comparison of typed worked/practice meanings: same source can bind another host key under a different catalog, so text equality alone is insufficient. The focused regression exercises that exact case and preserves prior knowledge. A final test-only minimum envelope/zero-text witness checks the promised inspectable failure even with no compile/text payload budget. These refinements affect only learning code/tests, leaving compiler metadata and all accepted host/item/checkpoint source laws unchanged after the accumulated run.

### M4 executed commands/counts and pending boundaries

All configure/build/native checks use the existing clone-local Debug C++23 GNU16 ASan+UBSan build through `/mnt/projects/cheryl-features/scripts/build --`, one job. Native CTest environment is command-local `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`, retaining address/undefined checks and the existing tracing-related LSan gap. Complete command/stdout/stderr/immediate exit logs live under `feature-development/magic/logs/M4/`.

| Log | Actual command after the required wrapper | Immediate result |
| --- | --- | --- |
| configure-01.log | `cmake -S projects/magic -B projects/magic/build -DCMAKE_BUILD_TYPE=Debug -DMAGIC_SANITIZERS=ON` | exit0, incremental existing configuration |
| build-01.log | `cmake --build projects/magic/build --parallel 1` | exit2: new test helper `failed` collides with the failure counter; compiler/host/learning libraries and old executables build first. This failure is preserved, not called a passing check. |
| build-02.log | `cmake --build projects/magic/build --target magic_learning_checks magic_learning_demo --parallel 1` | exit0 after test-only helper rename; both new executables build |
| accumulated-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1` | exit0,15/15 entries once, including all13 M1–M3 names; initial23 M4 contract cases and1 full teach-compose-cast caller |
| build-03.log | same two learning build targets | exit0; narrow duplicate-definition typed-key correction and regression |
| affected-01.log | `ctest --test-dir projects/magic/build --verbose --parallel 1 -R '^magic_learning_(contracts\|caller)$'` (saved command records the actual unescaped regex alternation) | exit0,2/2 entries,24 M4 cases plus1 caller |
| build-04.log | `cmake --build projects/magic/build --target magic_learning_checks --parallel 1` | exit0; test-only minimum-reserve/zero-text and overlong-rule witness |
| affected-02.log | `ctest --test-dir projects/magic/build --verbose --parallel 1 -R '^magic_learning_contracts$'` | exit0,1/1 entry, final25 M4 cases; production and caller unchanged |

Native total: one configure, four builds (three pass, one corrected test-harness failure), one accumulated suite and two affected suites. No resource/auth refusal or unchanged retry. Initial configure/build queueing retains the original sessions until admitted/completed; no cancellation/relaunch. No sanitizer finding. Unique CTest entries remain15, not18 due to reruns. Accumulated M1 counts remain35 compiler cases,4 ordinary callers,6 file fixtures and9 CLI processes; M2 remains37 host cases/6 reference callers; M3 now runs its final34 cases/3 callers in this accumulated invocation. Six new M4 authored verse/Common files are distinct from M1's six unchanged input fixtures. M4 final25 contract cases and one caller are separate counts, not extra CTest entries.

Fixture-specific exact positive/one-short thresholds: new edit work14823 vs14822 (helper slot/title/annotation included), tome work23357 vs23356, tome retained17860 vs17859 logical bytes, book reserved envelope12288 per configured slot, replacement plan bytes exact vsone-short. Entry count8/9, tightened1/2 and0/1, text8192/8193 plus tightened input length/one-over and zero text, title64/65, ID32/33, annotation256/257, work reserve12288/12287, revision1/next plus0/first, invalid registry bit256, all four learnable school productions and idempotent/conflicting identity definitions are exercised. Loop variations are not counted as separate named cases. Compiler-owned diagnostic spans and old Attempt/Entry/Plan handles outlive later edits and source/catalog/tome/book destruction. Tests check independent full Meaning and Common literals for worked/new/practice plans, exact clause alignment and actual before/release/onset/expiry health/mana/speed values. Queued casts and installed inscriptions keep their original9hp/12hp despite book edits to31hp/30hp; a real M3 checkpoint retains those original plans across all original owners' destruction and executes71hp total after restore.

Static/staged review separately verifies formatting/diffs, unchanged M1–M3 receipts/log bytes and unchanged reference host/item/checkpoint source/tests/callers. The compiler changes only owned requirement metadata, sizeof-based storage and one work unit/effect; all prior grammar/Meaning/Common and host behavior pass accumulated checks. New owner mutations are synchronous single-thread fixture operations; immutable handles support stable reads, with no mutable view API.

Material pending limits: no graphical spellbook/editor, full language pack, provider/NLP, disk book/knowledge save format, migration or integration into M3 world checkpoints. M3 checkpoints retain spell meaning, not book drafts or learned records. No human comprehension/enjoyment/playtest or production balance/security/Engine integration claim. Reference runtime remains spoken contact/G0 disks and immediate one-charge item-touch; ray/broader triggers/rearming remain unsupported. No crash/process/durable external exactly-once guarantee; deliberate whole-world rewind keeps M3's timeline semantics. Other platforms/toolchains/ABIs, leak checking under tracing, allocation fault injection/OOM and exact heap/RSS/CPU accounting remain unverified. Caller-retained immutable history is outside owner quotas; source-reviewed before-adoption exception preservation is not injected-failure evidence.

Next: commit this one coherent MAGIC-M4 checkpoint and remain idle. The finite controller alone may admit pinned MAGIC-M5 after clean/drained committed passing receipt/evidence. M5 must inspect this actual lesson-to-new-cast source, typed-key correction and saved passing logs before extending annular composition. No M5 implementation/scheduling begins here. Publisher owns ordinary feat/magic transport.
