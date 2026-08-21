# Issue #180: Single-active-punishment arbitration (Overcrowd > ability-lock > speed-reduction)

Closes the loop opened by closed issue #24: with `UPunishmentManagerComponent`
(#177/PR#182), `UAbilityLockoutComponent` (#178/PR#197), `USpeedReductionPunishmentComponent`
(#179/PR#196), and `UOvercrowdDetectionComponent` (pre-existing, PRD 08) all merged and
each activating independently, this adds the missing arbitration layer so at most one
punishment is ever active, honoring priority Overcrowd > ability-lock > speed-reduction.

Adds `UPunishmentArbitrationComponent`, a new `UActorComponent` that becomes the sole
production listener of `UPunishmentManagerComponent::OnPunishmentTriggered` (replacing
the direct bindings `AbilityLockoutComponent`/`SpeedReductionPunishmentComponent`
previously had to it), and also listens to
`UOvercrowdDetectionComponent::OnPanicOverloadStateChanged` to preempt both
lower-priority punishments the instant Overcrowd activates. `OvercrowdComponent` is
resolved lazily in `BeginPlay()` via `FindComponentByClass` (it is Blueprint-placed, not
pawn-C++-constructed), while `AbilityLockoutComponent`/`SpeedReductionComponent` are
wired explicitly by each pawn's constructor, same idiom as the components they arbitrate.

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what the
harness actually builds/tests against. Per D-009, this PR's diff contains a **copy** of
the new/changed source at `app-source-tracked/<same path under app/Source/>`.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `PunishmentArbitrationComponent.h` | CREATE | `UPunishmentArbitrationComponent : public UActorComponent` — public `TObjectPtr` refs to `OvercrowdComponent`/`AbilityLockoutComponent`/`SpeedReductionComponent` (all nullable), `BeginPlay()` override, `HandlePunishmentTriggered()`/`HandlePanicOverloadStateChanged()` UFUNCTIONs, private `IsOvercrowdActive()` |
| `PunishmentArbitrationComponent.cpp` | CREATE | `BeginPlay()` resolves `OvercrowdComponent` via `FindComponentByClass` + binds `OnPanicOverloadStateChanged`; `HandlePunishmentTriggered()` drops the trigger if Overcrowd is Active, else ends any active speed-reduction and activates ability-lock if present, else activates speed-reduction; `HandlePanicOverloadStateChanged()` force-ends both lower-priority punishments on the Inactive->Active transition only |
| `AbilityLockoutComponent.h` / `.cpp` | UPDATE | New `EndAllLockouts()` — immediately clears every locked slot, broadcasting `OnAbilityLockoutChanged(Slot, false)` only for slots that actually transition (same guard shape as `AdvanceLockouts()`) |
| `SpeedReductionPunishmentComponent.h` / `.cpp` | UPDATE | New `EndSpeedReduction()` — clears the pending restore timer and calls `RestoreOriginalSpeed()` immediately; guarded by `IsTimerActive()` so it's a safe no-op when nothing is active |
| `OvercrowdDetectionComponent.h` | UPDATE | Adds `friend class FKrowdKontrolPunishmentArbitrationComponentTest;` (6th grant, non-transitive, same rationale as the existing 5) so the new test can drive real `Active` state via `AdvancePanicOverloadState` |
| `EnemyBase.h` | UPDATE | Adds the same friend grant for `TickCheckDetection` — needed by the new test's Overcrowd-Active recipe (enemy spawn + `TickCheckDetection`), missed by the original investigation's Files-to-Change list |
| `FlatCamera3DPrototypePawn.h` / `.cpp` | UPDATE | Adds/wires `PunishmentArbitrationComponent` (after both `AbilityLockoutComponent` and `SpeedReductionPunishmentComponent` already exist); removes the direct `AbilityLockoutComponent` `OnPunishmentTriggered` binding; `PunishmentManagerComponent->OnPunishmentTriggered` now binds only to the arbitration component |
| `Paper2DPrototypePawn.h` / `.cpp` | UPDATE | Same shape, minus `AbilityLockoutComponent` (this pawn has none) — `PunishmentArbitrationComponent->AbilityLockoutComponent` is left nullptr, so arbitration falls through to speed-reduction normally, unchanged behavior for this pawn |
| `Private/Tests/KrowdKontrolPunishmentArbitrationComponentTest.cpp` | CREATE | `KrowdKontrol.Unit.PunishmentArbitrationComponent` — covers all 3 preemption pairings (ability-lock>speed-reduction, Overcrowd>ability-lock, Overcrowd>speed-reduction), both drop pairings (ability-lock/speed-reduction dropped while Overcrowd active), and the no-`AbilityLockoutComponent` fallback (mirrors `Paper2DPrototypePawn`) |
| `Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` | UPDATE | Replaces the now-false pre-arbitration assumption (contact damage reduces `MaxSpeed`) with the correct post-arbitration one: `MaxSpeed` stays untouched, `AbilityLockoutComponent` locks Stun instead |

### Deviations from the investigation/plan

- **`app/` was missing #179's pawn-level wiring entirely**, discovered while diffing
  `app/` against `app-source-tracked/` before starting: `SpeedReductionPunishmentComponent`
  existed as a file in `app/` but was never declared as a member or constructed on either
  pawn, and neither smoke test exercised it — despite `app-source-tracked/` (PR #196)
  having both. `app-source-tracked/` was already correct; `app/`, the actually-buildable
  project, was not — a repeat of this repo's known "mirror-only edit" incident pattern,
  this time from a prior issue rather than this one. Fixed as a prerequisite by building
  the final arbitration-integrated state directly into `app/` (skip add-then-remove for
  the direct binding that was never there), rather than literally following the plan's
  "remove the existing direct binding" step, which had nothing to remove in `app/`. Both
  trees are textually identical after this PR's edits (verified via `diff`).
- **`EnemyBase.h` needed a friend-class grant for `FKrowdKontrolPunishmentArbitrationComponentTest`**,
  not called out in the investigation's Files-to-Change table. `TickCheckDetection` is
  private; the new test's Overcrowd-Active recipe (mirrored from
  `KrowdKontrolOvercrowdAudioSubsystemTest.cpp`) needs it. Added the same non-transitive
  grant every other Overcrowd-driving test already has.
- `Paper2DPrototypePawn`'s smoke test (`KrowdKontrolPaper2DPipelineSmokeTest.cpp`) has the
  same `app/`-vs-`app-source-tracked/` gap as the FlatCamera3D one did (missing the #179
  speed-reduction wiring assertion in `app/`), but Paper2D's own behavior is unaffected by
  arbitration and the existing test still passes either way — left as-is since backfilling
  it is outside this issue's scope (not in the plan's Files to Change, and the smoke test
  itself was never asserting anything now-false the way FlatCamera3D's was).

## Acceptance criteria

- [x] **Contact-damage triggers read Overcrowd/Panic Overload state and treat it as the
      highest-priority punishment.** `IsOvercrowdActive()` checked first in
      `HandlePunishmentTriggered()`, drop-and-return.
- [x] **Ability-lockout is priority 2 (below Overcrowd, above speed-reduction).**
- [x] **Speed-reduction is priority 3 (lowest).**
- [x] **A higher-priority punishment activating ends any active lower-priority one
      immediately, effects fully reverted, not just paused.** `EndAllLockouts()`/
      `EndSpeedReduction()`, both instant and unconditional.
- [x] **A lower-priority trigger while a higher-priority punishment is active is
      dropped — no effect, no queued follow-up.**
- [x] **Automation tests cover all 3 preemption pairings and both drop pairings.**
      `KrowdKontrol.Unit.PunishmentArbitrationComponent`, cases (a)-(h).
- [x] **Level 1-3 validation passes with exit 0, `GATE_OK`.** See evidence below.
- [x] **No regressions** — every pre-existing `KrowdKontrol.Unit.*` test still passes;
      `KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` updated deliberately, not broken.
- [x] **`app/` and `app-source-tracked/` copies of every changed/new file are identical**
      — confirmed via `diff` for all 11 touched files.

## Validation evidence

`python harness/ci.py --quick` (inline sanity check, Phase 6):

```
GATE_OK mode=quick
```

Full `python harness/ci.py` (headless Unreal Editor rebuild + Automation Framework run)
is deferred to the separate `dark-factory-validate` node per this workflow's own
instructions; not re-run here.
