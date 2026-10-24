# Procedural-generation next action

PROCGEN-U5 is implemented and verified from accepted U4
`138f8e0d6b88736453643d58a93073fa936619fd` on `feat/procedural-generation`.
Private owned cardinal distance fields, exact threshold masks and usable-region
counts work on generated Cave and literal binary2D inputs. Outside is solid,
perimeter floors distance1, solids0; minimum0/1 includes every floor. No input
smoothing/carving/filtering mutation or generator replay change.

Preserved GNU/Linux Debug/C++23/UBSan build passed the accumulated nonempty CTest
selection ONCE: **52 passed,0 failed,0 skipped**,49 cases/22276 assertions.
Includes all43 prior entries (9479 assertions) and eight clearance cases (12797),
plus maze8/8, cave2/2 and clearance2/2 consumer demonstrations. Complete logs and
immediate exits: `builds/procedural-generation/logs/u5/`. PROCGEN-019–022 in both
records retain exact definitions, storage/work accounting and source evidence.
Normal queue wait completed in its original session; no admission refusal.

Ready receipt is committed with this checkpoint:
`feature-development/queue/three-commits-20261024/PROCGEN-U5.json`.
It reports executed U5 checks, not automatic coordinator source acceptance.
Next concrete action belongs to the finite controller: after this run drains,
validate the clean descendant checkpoint/receipt/logs, then materialize and resume
preassigned PROCGEN-U6 (bounded connecting tunnels). This worker stops here and
starts no later milestone, sibling/background work or publication command.
Assigned-branch checkpoint transport remains with the coordinator publisher.

Assumptions/confidence: exact point-cell Manhattan/exterior/threshold contracts
are verified (high); private analysis API reuse is a reversible choice (medium).
No unresolved blockers. Negative thresholds and malformed input reject; high
thresholds validly select none; zero extents/empty occupancy are invalid per the
positive Domain contract. Thresholding may split usable regions without removing
source floors. Accounted payload peak4096+17*C is not exact RSS; complete work is
15*C+6*usable. Other platforms/compiler, ASan, performance and public integration
remain unverified. No Euclidean/circular-actor/physical collision guarantee.

For retained layers, keep exact source Domain/occupancy identity plus analysis
family/version/minimum and coordinate/exterior convention. A changed source
invalidates its field. generation-storage.md chooses no persisted recipe, exact-
base witness format, save policy or sibling adapter here. U6/U7 remain queued;
advanced maze controls, broader worlds, physics and AI remain outside this unit.
