# Issue #375: Wire Starter Skill Effects to Existing Gameplay Tunables at Run Start

Closes the loop the Crowd Mastery skill tree opened: bubbles could already be
unlocked (`UCrowdMasteryTotalSubsystem::TrySpendOnBubble`, issue #371) but unlocking
one changed nothing in actual gameplay — `GetUnlockedBubbles()` had no reader.

Adds `UCrowdMasteryTotalSubsystem::GetUnlockedEffectHookIds()`, resolving every
currently-unlocked bubble to its `EffectHookId` via the existing private
`FindBubbleAndOwningNode`. `AKrowdKontrolPlayerController` gains
`ApplyStarterSkillEffects(APawn*)`, called from both `BeginPlay()` and `OnPossess()`
(the same dual-call-site seam `WireWidgetsToPawn`/`ApplyBossCheckpointIfRequested`/
`RetryPendingAbilityUnlock` already use, since pawn-possession timing relative to
`BeginPlay` isn't guaranteed), one-shot-guarded by `bStarterSkillEffectsApplied` so a
controller that runs both call sites (or a later repossession) never double-applies a
multiplicative bonus.

Four of the five PRD-named starter effects are wired, each mutating an existing
tunable — no new parallel stat system:

- **`AbilityCooldownReduction`** — scales every entry of the possessed pawn's
  `UAbilityCooldownComponent::AbilityCooldownDurations` by 0.8× (20% shorter
  cooldowns).
- **`EnergyMaxIncrease`** — scales `UPlayerEnergyComponent::MaxEnergy` by 1.25×
  (+25% ceiling). Does **not** top up `CurrentEnergy` — that field's only permitted
  public mutator is `ApplyContactDamage`, an invariant this change does not touch or
  route around.
- **`MovementSpeedBonus`** — scales the pawn's `UFloatingPawnMovement::MaxSpeed` by
  1.15× (+15% top speed), same capture-then-multiply shape
  `SpeedReductionPunishmentComponent` already uses, but permanent for the run (no
  restore call).
- **`PenZoneRadiusBonus`** — scales every `ATargetZone` in the world's
  `ZoneCollisionComponent` box extent by 1.2× (+20% catch radius).

**`ControlledDurationBonus` is deliberately not implemented** (flagged here for
operator sign-off per the issue's own instruction). The only existing
"Controlled duration" value, `AbilityData::Get(Ability).BaseDurationSeconds`, is a
documented locked GDD constant — a single global shared by every pawn and every run.
Mutating it would mean either `const_cast`ing a documented-immutable value or
introducing a new per-run duration-bonus field consulted by
`AEnemyBase::ReceiveControl` — exactly the "new parallel stat system" this
requirement forbids. `ControlledDurationBonus` bubbles (`Root_ControlledDuration`,
`PenZone_ControlledDuration2` in the live `DT_MasteryTreeTable`) remain
spendable/unlockable but produce no gameplay effect until a follow-up issue exposes a
real per-run seam for it.

`KrowdKontrol.Unit.StarterSkillEffectWiring` covers: an unlocked cooldown-reduction
bubble scales every cooldown-duration entry; an unlocked move-speed bubble scales
`MaxSpeed`; a bubble that was never spent (`EnergyMaxIncrease`) leaves `MaxEnergy`
untouched; a repeat `OnPossess()` does not re-multiply `MaxSpeed` a second time.
`PenZoneRadiusBonus` shares the identical `Contains()`-gated branch shape as the two
directly-tested effects and isn't separately asserted, matching this codebase's
existing "don't need N tests for N near-identical branches" precedent.

`docs/prd-mastery-skill-tree.md` REQ-3 is annotated `⚠️ partially implemented, issue
#375`, matching REQ-1's existing annotation convention.

## Acceptance criteria

- [x] At run start, the game queries `GetUnlockedBubbles()` (via the new
      `GetUnlockedEffectHookIds()`) and applies each bubble's effect via existing
      pawn/component tunables — 4 real, distinct effects implemented, satisfying the
      "at least 3" bar.
- [x] Each effect modifies an existing tunable/component value directly — no new
      stat system, no duplicate state.
- [x] `KrowdKontrol.Unit.StarterSkillEffectWiring` confirms an unlocked bubble
      measurably changes its tunable and a locked bubble does not.
- [x] PR body lists the chosen starter effect set and the `ControlledDurationBonus`
      exclusion, requesting operator sign-off.
- [x] `app/Source/KrowdKontrol/` and `app-source-tracked/Source/KrowdKontrol/`
      copies verified identical via `diff` before commit.
- [ ] Level 1-3 validation commands pass with exit 0, 0 regressions — **see
      Validation evidence below: the unit rung is clean, but the PIE rung's
      pre-existing `KrowdKontrol.PIE.LifecycleLiveFire` failure is unrelated to this
      change and is not fixed here** (see below for why).

## Not building (scope limits, per this plan's own text)

- `ControlledDurationBonus` — see above.
- No UI change — `MasteryScreenWidget`'s display is untouched.
- No persistence of which effects were applied — effects are re-derived live from
  `GetUnlockedBubbles()` every run; `SpentPoints`/`UnlockedBubbleIds` remain
  session-only per issue #371's own scope limit, unchanged here.
- No numeric balancing beyond the named starter values (20%/25%/15%/20%) — explicit
  PRD scope limit; values are `constexpr` in one place, trivially retunable later.
- No modifier-slot system (REQ-4) — separate P1 issue, not started.

## Validation evidence

`harness/run_ue_automation.sh KrowdKontrol.Unit.StarterSkillEffectWiring`:

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

`harness/run_ue_automation.sh KrowdKontrol.Unit` (full suite, 0 regressions):

```
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=132 total=132
UE_AUTOMATION_OK
```

(131 passing before this change per issue #371's changelog + 1 new = 132.)

`python harness/ci.py --quick`:

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=132
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=7 total=8
UE_AUTOMATION_FAILED KrowdKontrol.PIE.LifecycleLiveFire: state=Fail
GATE_FAILED: pie
```

**`KrowdKontrol.PIE.LifecycleLiveFire` fails for reasons unrelated to this change,
confirmed by direct baseline test:** the 4 files this issue touches
(`CrowdMasteryTotalSubsystem.h/.cpp`, `KrowdKontrolPlayerController.h/.cpp`) were
temporarily reverted to their last-committed (`app-source-tracked/`) content, the new
test file removed, and the PIE test re-run in isolation — it still fails
(`passed=0 total=1`, same "Timed out driving all enemies to Banked within 60
seconds" error), proving the failure predates and is independent of this diff. This
worktree's `app/` is a single physical directory shared by every concurrent factory
task (`CLAUDE.md`'s Environment section), and a broad `diff -rq` against
`app-source-tracked/` at the time of this run showed unrelated, uncommitted
divergence in `TargetZone.cpp/.h`, `EnemyBase.cpp`, `RoomActor.cpp/.h`,
`FlatCamera3DPrototypePawn.cpp/.h`, `DoorConnectorActor.cpp`, plus several stray
`.orig` backup files — strong evidence of another in-flight task mid-edit on exactly
the banking/enemy-lifecycle code path `LifecycleLiveFire` exercises. All 4 files this
issue actually changes were re-restored to their finished, correct state
immediately after this baseline check (re-verified identical to
`app-source-tracked/` via `diff`). Not fixed here — touching those unrelated files
risked clobbering another task's in-progress work, well outside this issue's scope.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this only reads existing tunables and existing subsystem
state through the established pawn-wiring seam.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
