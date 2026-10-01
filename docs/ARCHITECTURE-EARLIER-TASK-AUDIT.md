# Earlier-task audit checkpoint

1 October 2026. This is a partial review of tasks 0–6 and their integration with
tasks 7–8 against the second-pass work order, starting from the user-pushed `8f796148e64d3dde6faf29b9333893133eb9cceb`.
Earlier completion labels describe implemented source scope. They do not establish
that the implementation is correct, that every path was reviewed, or that the
acceptance criteria passed. The broader audit remains open alongside implementation, as subsequently requested.

The subsequent explicit audit request pauses further task-9 completion work.
[ARCHITECTURE-SUBTASK-AUDIT.md](ARCHITECTURE-SUBTASK-AUDIT.md) records every completed
task/subtask and dependent paths; the finding history below is not full sign-off.

## Findings and prepared changes

### A1: event invalidation was published after registry removal

Previously, close detached the registry under its mutex and invalidated listeners
after unlocking. Another close could see an empty registry and return before the
first closer closed the invocation gates. A queued task could then enter after a
close had returned. Concurrent unregister also detached an entry before closing
its gate, letting close miss that entry. The earlier duplicate-unregister fix did
not solve either interaction.

Checkpoint 35 invalidates before removal while holding the registry mutex. The
lock order is registry then listener. Invocation and completion release the
listener mutex before user code and never acquire the registry while holding it.
Local/detached owners retain callbacks until both locks have been released, so
destructors can reenter. Already-running callbacks still finish without making
close wait for them.

Two prepared scenarios cover concurrent closers with an in-flight callback and a
queued callback, and callback-capture destruction which reenters close. They
exercise the public lifecycle contract; they do not force the old intermediate
detach/invalidate window deterministically. The race finding rests on the lock
and interleaving review, not a claim of an executed reproduction.

### A2: polling saturation could overflow before making its comparison

PollingBacklog computed `time_point::max() - completed_at` before checking spacing.
A timestamp before the epoch can make that subtraction exceed the signed clock
duration range. A steady clock's epoch is unspecified; nonnegative timestamps
are not an API guarantee.

Checkpoint 36 compares completion against `max - nonnegative_spacing`, then adds
only when representable. Prepared scenarios cover near-limit completion,
maximum spacing, zero spacing, and retained observations after saturation. The
current InputBindings baseline comes from the real clock, so these fixtures do
not provide a synthetic negative-epoch observation. That branch was checked by
arithmetic review; execution and a suitable controlled-input fixture remain open.

### A3: worker integration coverage was narrower than the implemented policies

Existing sources covered a concurrency cap of one and one required CPU, but not
parallel cap isolation or effective-mask restoration between groups. Checkpoint
37 adds a cap-of-two scenario with held jobs and independent-group progress, and
a Linux pinned/inherited/pinned mask sequence on one physical worker. A bounded
future wait limits failure cleanup; latches establish the cap scenario's state.
These are coverage additions, not evidence of a discovered scheduler failure.

### A4: status documentation lagged behind implementation

Dispatch documentation still said timing configuration was future work. The
residency table described pipeline/material contracts as wholly pending even
though their strong ownership and native builders now exist. Checkpoint 38
corrects those boundaries and distinguishes source scope from audit sign-off.
The attached-stage retention issue found during task 7 was already fixed in
checkpoint 32; it is further evidence that the earlier task-6 review was incomplete.

### A5: queued payload preparation could report cancellation and propagate separately

Checkpoint 39 moves ticket allocation and owned-payload construction into the same
failure boundary as posting. A payload-copy failure records the original exception
on an existing ticket before its destructor can report cancellation; allocation
failure without a ticket reports through the active listener's sink after unlocking.
Prepared throwing-copy/target scenarios cover sink reentry and recovery. These have
not been executed, and the broader queued lifecycle combinations remain open.

### A6: fixed-state integration exposed depth-clear mask dependence

Pipeline draws can disable depth writes. OpenGL depth clearing obeys that retained
mask, so checkpoint 40 enables writes before clearing; the next draw reapplies its
own complete policy. This is a state/ownership review finding, not an observed GPU
failure. Native acceptance remains open.

### A7: a worker stream advertised a pump before submission succeeded

Checkpoint 46 serializes the first pump's pool submission with stream publication.
Previously another producer could return accepted while the initiating submit was
still fallible. Rejection then destroyed both requests on the initiating producer,
including another listener's cancellation sink under the initiating listener's
posting lock. Reentry could block on that lock. Checking group closure under the
stream mutex also excludes the draining pump's final empty-queue/completion step.
The fix releases only the failed initiating capture after unlocking and keeps the
stream-to-pool lock order. Prepared sources cover close with queued accepted work,
reentrant rejection reporting, and independent stream progress; they do not force
the old submission interleaving. Native pump-rejection acceptance remains open.

### A8: cache insertion moved its final local candidate owner into fallible work

Checkpoint 47 keeps a local strong candidate owner through try_emplace. Previously
node construction could move away that last owner, then insertion/rehash failure
could destroy the node under the cache write lock. A final deleter inspecting that
cache could deadlock. Keeping the argument's owner makes final cleanup occur after
lock unwinding, including duplicate and failed insertion. Replacement already
inserts an empty slot before exchanging ownership, and clear detaches the map.
Prepared generic-cache sources cover duplicate-candidate deleter reentry and a
controlled hash-rejection failure. Allocation/rehash failure was checked by owner
and exception-scope review; those fixtures do not force allocation failure.

### A9: static texture unbinding bypassed native ownership and inherited a unit

Checkpoint 48 replaces Texture::unbind() with instance-bound unbind(unit), checking
the owning live/current context and unit limit before selecting and clearing that
unit. Constructor cleanup supplies unit zero explicitly. Other binding paths already
check their owners/domains. Prepared native recording sources cover unrelated
active-unit state and rejection on foreign/missing/closed contexts or invalid units.
No real OpenGL execution has occurred.

### A10: an accepted pump's early policy rejection could clean up on its producer

Checkpoint 46 serialized pool submission but still retained a local shared pump
owner. If native policy rejected the accepted job before callback entry, that
local owner could become the last owner after the stream lock was released. A
second listener could join in that window; the producer's pump destructor could
then destroy its cancellation ticket under the first listener's posting lock.
A sink reentering that first listener could deadlock.

Checkpoint 55 moves cancellation to a move-only guard owned by the submitted
callable. A preparing/accepted/cancelled atomic handshake distinguishes loss before
publication from loss afterward. Before publication, destruction only records
cancellation; the producer withdraws its sole request while excluding other
producers, then releases it outside the stream lock. After publication, the
unentered callable owns stream abandonment; the producer's shared owner has no
cleanup destructor. Synchronous submit exceptions retain their original error.

Checkpoints 56–57 prepare a private submission seam and scenarios for loss before
publication, loss after two listeners join, and synchronous throwing submission,
including cancellation-thread reporting and same-listener reentry/recovery. They
are uncompiled/unexecuted, model callable loss without an OS adapter, and do not
force every old intermediate producer interleaving. The finding rests on source
ownership/interleaving review; native policy and startup failure acceptance stay open.

### A11: rejected queued payload destruction could reenter under the posting lock

The local ticket kept cancellation reporting outside the listener's posting lock,
but the deferred callable owned its copied payload separately. A target returning
false or throwing could destroy that callable before the offer returned, releasing
the payload while the bus still held posting. A copied payload destructor or final
deleter which dispatched to the same listener would try to reacquire that mutex.
The A5/A7/A10 reporting fixes did not protect this separate capture owner.

Checkpoint 58 puts the payload inside the ticket. Its producer-side local owner
now pins both reporting and payload destruction through the posting scope. Copy
preparation still occurs before posting and inside the original-error boundary.
The prepared scenario rejects once by returning false or throwing, redispatches
from only the copied payload's destructor, then explicitly drains the accepted
replacement. It checks one original failure, one copy release, and recovery.
This is a source ownership/reentry finding; the scenario is not compiled or run.

### A12: native texture and buffer upload failures still published logical resources

Texture/VAO construction checked generated IDs and registration, but treated
glTexImage2D, glGenerateMipmap and glBufferData as unconditional success. A driver
error could leave a constructor returning a resource without its requested native
storage. The legacy mesh path had the same gap for either index or vertex storage.

Checkpoint 60 checks native errors before creation and after generation, storage,
mipmap and attribute-layout setup. A preexisting error rejects before generation
instead of silently clearing it and attributing it to a new upload. A new error
throws with its code before logical publication. Generated but untracked IDs use
the existing guarded discard path; tracked IDs retire on constructor unwinding.
The atlas upload restores unpack alignment before checking failure and never
generates mipmaps after a failed base upload.

Prepared recording sources cover both mesh upload positions, 2D storage failure,
atlas alignment/mipmap suppression, mipmap failure, preexisting errors and
generation errors with an untracked ID. They compare every generated kind/ID
against exactly one collected or discarded kind/ID. These are synthetic, uncompiled
and unexecuted fixtures. They do not prove recovery from driver memory exhaustion:
[OpenGL 3.3 section 2.5](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf)
leaves native state undefined after OUT_OF_MEMORY, so storage queries cannot
replace error reporting. Context loss and allocation-failure evidence remain open.

### A13: native program construction and reflection errors were not checked

The program builder checked COMPILE_STATUS and LINK_STATUS but missed native
errors from source upload, compilation, attachment, linking, stage detachment
and status queries. In particular, a failed attachment could be ignored before a
later link, and a failed detach could retain delete-marked stages. Reflection
count/length queries initialized their outputs to zero, so a rejected query could
be interpreted as an empty interface. Optional parameters could then conceal the
failure during pipeline construction.

Checkpoint 65 moves the production builder behind a private source entry shared
by the provider and recording fixtures. It rejects a preexisting native error
before creation, then checks creation/source/compile/attach/link/detach/status and
diagnostic boundaries before adopting or publishing the program. Linked-status
and reflection count/length/entry/location queries also reject native errors.
Generated but untracked objects remain guarded; a tracked program is adopted
once, and later logical allocation/reflection failure retires it only when its
last strong owner releases it.

Seven prepared fixtures cover nine native construction boundaries, logical
compile/link failure, unreadable later-stage cleanup, rejection before creation,
transient stage destruction, foreign-thread last release, lost-current failure
cleanup, and failed-reflection/recovery with a retained program. The recorder
models stage deletion marking separately from actual detachment/program deletion;
its kind/ID checks require exactly one deletion request and actual destruction
for each generated object in the available-context scenarios. The lost-current
scenario instead requires no cleanup calls and preserves the original failure;
platform context destruction owns untracked remnants.

These sources are uncompiled and unexecuted. They establish neither real driver
cleanup nor recovery after memory exhaustion/context loss. The native-error and
stage-lifetime rationale follows [OpenGL 3.3 sections 2.5 and 2.11.2](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf).
Allocation injection, additional resource categories and combined runtime
failures remain open; seven fixtures do not close tasks 6–7.

### A14: font publication bypassed the retained cache owner

FontMgr inserted with a moved STBFont owner directly under its cache write lock.
A node/rehash failure could destroy the moved insertion candidate and its backend
resource owners before releasing that lock. This bypassed A8's shared publication
helper. Assigning the default-font path after insertion could also allocate and
fail after the font was already cached; a later load would skip the existing key
without repairing the missing default selection.

Checkpoint 67 routes font publication through the common retained-owner helper.
A potentially allocating default-path copy is prepared before constructing the
font. A nonthrowing metadata callback then selects that prepared path within the
same publication lock, only after insertion succeeds. The candidate stays pinned
until the lock unwinds; neither an allocation failure nor a duplicate insertion
can run its final deleter there. The callback must not throw or reenter the cache.

AssetMgr gains an optional allocator template argument and a protected allocator
constructor, retaining std::allocator for every existing production cache. Three
prepared fixtures use a per-map allocator to reject actual node or multiple-object
bucket allocation requests. They cover publication metadata remaining uncommitted,
replacement insertion failure, original-generation retention, destructor reentry,
recovery and stateful allocator clear. Clear preserves allocator equality and
checks the allocator under a shared lock before constructing the detached map;
values still release after the write lock. No global new override or extra test
target is introduced. These fixtures are uncompiled and unexecuted, and do not
establish a heap-exhaustion result or every standard-library allocation strategy.

Checkpoint 68 prepares provider-side geometry rejection through the common owned
CPU-buffer upload wrapper and Graphic construction. It also prepares partial
sprite/tileset upload, unchanged Loader metadata after failure, retry and retained
geometry across explicit cache clear. Those two scenarios use synthetic resources;
they do not inject failures inside native registration, font atlas creation or the
runtime's startup/shutdown sequence. Such cases remain distinct open audit work.

### A15: texture queries consumed output before checking errors

The anisotropy-limit query wrote into an uninitialized float that was immediately
passed to glTexParameterf. A failed query could leave that output untouched, so
the later upload error check came after an invalid C++ read. The unpack-alignment
query similarly allowed a failed query to restore a guessed value; a context using
alignment eight could be changed to four. Binding-limit queries also lacked an
immediate native status check.

Checkpoint 71 initializes the float and checks each query before consuming its
output, and checks sampling configuration before querying alignment/uploading.
Two recording fixtures prepare unchanged failed-query outputs, no anisotropy
parameter/image/mipmap use after failure, unchanged unpack alignment and one-time
texture retirement. They are uncompiled and unexecuted; real driver recovery and
context loss remain acceptance work.

Checkpoint 70 separately prepares five native registry/program allocation and
allocator-retention scenarios through private production entries. A scoped owned
memory resource rejects actual entry-vector growth or logical control-block
allocation. Live/pending entries survive failed growth; free-slot reuse and shutdown
need no new entry allocation. A later logical failure retires the adopted program
without duplicate untracked discard. Retained handles and weak control blocks keep
their allocator alive through final deallocation. Default production allocation
remains fixed new/delete for entries and make_shared for logical programs. These
sources do not close font bake/atlas, every native wrapper's allocation boundary,
combined runtime failure or actual heap-exhaustion acceptance.

### A16: worker shutdown allocated another group-owner vector

EngineContext::finish_workers copied worker_groups_ under its mutex before waiting.
That copy could allocate while the runtime was cleaning up a construction/allocation
failure. The runtime could retain its original error and continue adapter cleanup
after this new failure, leaving the context destructor to attempt the same fallible
copy again. Group ownership already lives in the context; a second vector was
unnecessary once submissions were closed.

Checkpoint 75 requires the existing submission-close barrier before finishing,
then borrows a span of the retained member groups while draining outside the mutex.
make_worker_group checks the same closed flag under that mutex before changing
the vector or owned pool. Both runtime cleanup paths and context destruction close
submissions first. Owned-pool shutdown still closes/joins its threads; an injected
root's unrelated groups remain available. This removes the deliberate vector
allocation, without claiming recovery from every mutex/native or heap failure.

Two prepared runtime fixtures combine resource allocation failure with an accepted
worker upload, pending simulation cancellation and later producer cleanup failure,
or a failed worker upload observed during game cleanup followed by renderer cleanup
failure. Both sequential and concurrent modes are covered in source. The application
observes the worker future; runtime shutdown does not silently consume its exception.
These fixtures are uncompiled/unexecuted and do not inject OS startup/affinity errors.

Checkpoint 73 separately extracts STBFont's unchanged baked upload sequence into
a private helper. Three prepared scenarios cover rejected geometry/CPU ownership,
atlas throw/null with completed glyph cleanup, and successful copied pixels,
vertices, metrics and retained resources. Checkpoint 76 prepares size/missing/empty
file rejection before provider calls; parsing/baking valid font data is not exercised.
Checkpoint 74 adds actual registration storage rejection at texture, flat VAO/buffer
and every legacy mesh slot, including earlier adopted owners and repeat collection.
Padding fills spare capacity only; no vector growth factor or global new override
is assumed. Full format/declaration and broader combined startup/policy/native
failure review remain open, alongside executed acceptance.

### A17: early runtime startup exits bypassed execution cleanup

Both runtime modes resolved EngineContext::window outside their guarded startup.
A missing active window threw before the staged cleanup could close context groups
or settle accepted work. Both modes also returned immediately when stop had been
requested before run, although begin_session had already reserved the single-use
context. Retained groups could keep accepting jobs after that runtime ended.

Checkpoint 78 resolves the window inside the guarded block and adds a shared
unstarted-session cleanup path. A pre-start stop closes group submissions and both
mailboxes, then finishes workers without starting adapters. Neither mailbox has
opened, so no accepted platform continuation needs pumping in that branch. The
missing-window branch uses the ordinary cleanup and preserves its original error.
Two fixtures prepare both exits with retained groups/work/captures and closed targets.

Checkpoint 79 adds context-owned policy failure on an injected pool and partial
renderer/input initialization followed by later cleanup failures. Both modes are
covered; policy-failed asset preparation never reaches upload and unrelated pool
groups remain available. Checkpoint 80 ends the fixture's runtime lifetime before
destroying its borrowed context and clears callbacks that borrow that context.
No OS startup/affinity, native driver or real-context failure has been executed.

Checkpoint 77 prepares the font bake retry gap through a private template shared
with production. Production retains its default allocator, dimensions and stb call;
fixtures provide zero/negative status, the maximum-size failure and an actual scoped
CPU bitmap allocation rejection. Storage releases before those failures escape.
These three fixtures do not parse/rasterize fonts or inject stb's internal allocations.
The identified category ownership gap under 6.2 is reviewed in source. Broader
runtime startup/native-context composition and full formatting remain open, as do
type/link, test and native acceptance gates. Seven new fixture functions remain
uncompiled and unexecuted; batch duration is not completion evidence.

### A18: initial OpenGL context acquisition bypassed startup cleanup

OpenGLRenderer::initialize called make_current before entering its initialization
try/catch. A compatible adapter could become current and then throw; runtime cleanup
called deinitialize, which returned because initialization had not published. The
renderer never attempted release for that partial acquisition.

Checkpoint 84 moves initial acquisition into the guarded block. Release is attempted
without replacing the original failure, including when acquisition never made the
context current or release itself rejects. One prepared fixture covers those cases,
requires zero procedure lookups/no published resource domain, and confirms later
renderer cleanup/destruction does not repeat initialization cleanup. No real context
or GL loader is exercised in this fixture; all new fixtures remain unexecuted.

Checkpoints 81–82 prepare the earlier controlled runtime startup composition gap:
owned-root partial startup rollback in both modes and dedicated simulation-thread
construction failure with accepted worker upload, unbound simulation cancellation,
and later cleanup errors. Private per-instance factories leave direct production
construction in place by default. Two older fixtures correct borrowed context lifetime.
Checkpoint 83 composes native generation/upload failure with missing current context,
recovery, abandonment, retained owners and foreign/late release. Unadopted IDs with
no current context remain native-context destruction's responsibility. Checkpoint 85
corrects declaration order in seven types, preserving member sequence and visibility.
Five new fixture functions are uncompiled/unexecuted. Bounded 4.6 startup/failure
source review is recorded; full 0.3 formatting and broader 6.6 native compositions,
including GL-state startup publication and program/material/frame/runtime dependents,
remain open before task 9. Static checks are not type/link or runtime sign-off.

## Coverage at this checkpoint

The 46–51 continuation starts from pushed `7f042e9d91e28a9addb8d66284e9cdb3ff6938fd`,
which matches the previous local 39–45 tree. Source review spans both runtime modes,
dispatcher queue/capture ownership, event lifecycle and lock order, worker scheduling
and native policy, scheduler/input arithmetic, cache publication, native construction
and binding, material resolution, CPU submission, and frame retention/recycling.
A7–A9 are source fixes found by that review; A10 extends the event-pump failure
review in the current 55–57 continuation from pushed checkpoint 54,
`b7ada603e1d7078f2cc1ed89015d07e3541eb4a0`. No additional handoff defect was found
in the inspected frame state transitions; that is a limited review result.

Checkpoint 49 adds coordinated frame/reload/failure cleanup sources in both modes.
Checkpoint 50 adds required CPU-mask revalidation after a job changes its own mask.
The detailed path/evidence/remaining-acceptance map is in
[ARCHITECTURE-CONVERGENCE-REVIEW.md](ARCHITECTURE-CONVERGENCE-REVIEW.md).

## Validation and continuation

Changed C++ files receive syntax-tree inspection; whitespace, API/include, and
ownership/lock review accompany the changes. Combined continuation, numbered
increments, and original-base mailbox replay check exact source trees and
ordered author/date/messages before delivery. These checks do not establish type
correctness, linking, native execution, or race freedom. No configuration, build,
regression execution, real-context acceptance, or assistant remote write occurs.

Keep the partial audit open. Controlled native affinity/thread-start failure,
allocation/rehash failure, the old pump-submission window, real context loss, and
cross-task producer/failure combinations remain acceptance or coverage gaps.
Recovery checkpoints 52–53 prepare overlapping weighted affinity, capture-deleter
reentry, failed-shutdown recovery, and abandonment/late-release coverage. Those
sources also remain unexecuted. Do not infer exhaustive review or correctness from batch duration.

Checkpoint 62 prepares the previously missing controlled query/set/readback,
preferred fallback and partial thread-start failure sources through a private
per-pool native adapter. Checkpoint 63 links an accepted pump's policy rejection
to worker-side listener cancellation and reentrant recovery using production
WorkerGroup submission, and adds simultaneous producers/held-copy invalidation.
These sources narrow the identified coverage gaps without executed OS/runtime
acceptance, exhaustive producer interleavings or closure of the broader audit.
