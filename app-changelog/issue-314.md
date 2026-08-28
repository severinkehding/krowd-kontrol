# Issue #314: Add a KrowdKontrol.PIE regression scenario for post-contact enemy recovery

Issue #313 (merged via PR #336) added `AEnemyBase::TickAttackDuration()` — a
timer-based, ability-independent `Attack -> Alert` exit — but its own bundled
regression coverage lives entirely in `KrowdKontrol.Unit.EnemyBase`, driving the
transition through the `TickCheckDetection`/`TickAttackDuration` friend hooks
directly, in a `CreateNewMap()`/`NewObject` world that never runs a real
begin-play/tick/PIE-session path. Per `harness/README.md`'s own stated rule ("if a
feature's failure mode involves begin-play order, ticking, ... it needs a
`KrowdKontrol.PIE.` scenario test"), this issue adds that companion test.

## What was added

`KrowdKontrol.PIE.PostContactAttackRecovery`
(`Source/KrowdKontrol/Private/Tests/KrowdKontrolPIEPostContactAttackRecoveryTest.cpp`) —
opens `L_Level01` in a real PIE session, resolves the first `AEnemyBase` actor in the
level (waiting for its owning room's activation if it has one, mirroring
`KrowdKontrolPIELifecycleLiveFireTest.cpp`), teleports the real player pawn onto it so
the real `Tick() -> TickCheckDetection()` proximity check (never a direct/friend call)
drives `Idle -> Alert -> Attack`, then retreats the player pawn well outside detection
range (mirroring issue #313's own "player retreats out of range" bug report) and
asserts, purely through real per-frame ticking with `ReceiveControl()` never called,
that:

- the enemy's `Attack` state reverts to `Alert` on its own,
- `OnEnemyAttackExpired` fires exactly once, and
- the enemy visibly resumes chase movement toward the retreated player afterward.

No production code changes — this issue is scoped to test coverage only. No
lifecycle/friend method (`TickCheckDetection`, `AdvanceToAttack`,
`TickAttackDuration`, `GetAttackDurationSeconds`) is called directly anywhere in the
new test.

## Acceptance criteria

- [x] A `KrowdKontrol.PIE` scenario test exists that drives an enemy into contact with
  the player pawn under normal play conditions (real `Tick()`-driven proximity, no
  direct friend-hook calls) and asserts it is moving/acting again within a bounded
  time window, with no `ReceiveControl()` call anywhere in the test.
- [x] The test lives alongside the existing `KrowdKontrol.PIE` scenario tests
  (issues #236/#237), using the same file/test-group/setup-teardown conventions.
- [x] The test would fail against the pre-#313 codebase and passes against the
  current (post-PR #336) codebase — verified logically (`Attack` had zero exit but
  `ReceiveControl()` before the fix; this test never calls it).
- [x] `app/` and `app-source-tracked/` copies of the new file are byte-identical.
- [x] No production code (`EnemyBase.h`/`.cpp` or any concrete enemy subclass) is
  touched.

## Validation evidence

Full validation gate (`harness/ci.py --full`) reports `GATE_OK`:

- `PIE_PASSED tests=6` (one higher than PR #336's baseline of 5, confirming the new
  test is picked up by the suite)
- `UE_AUTOMATION_RESULT passed=1 total=1` — confirms
  `KrowdKontrol.PIE.PostContactAttackRecovery` compiled and ran under
  `UnrealEditor-Cmd.exe` on the Windows-side Editor/UnrealBuildTool, not just an
  inline sanity check from WSL.
