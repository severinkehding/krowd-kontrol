# Issue #59: Trigger First Gizmo Contact After the Player's First Successful Stun Use

Adds `UGizmoFirstContactComponent` (`app/Source/KrowdKontrol/GizmoFirstContactComponent.h/.cpp`),
a small component attached to `AFlatCamera3DPrototypePawn` alongside `UAbilityCastVFXComponent`
(issue #67, the same pattern this mirrors). It binds to `UAbilityCastComponent::OnAbilityCastApplied`
in the pawn's constructor and, the first time (and only the first time) the player successfully
casts Stun against an enemy, lazily registers a new `FirstContact.Stun` bark with
`UGizmoNarrativeSubsystem` and calls `TriggerBark`. The subsystem's own no-replay guarantee
(flip-before-broadcast, silent no-op on an already-fired ID) means the new component needs no
cast-counting state of its own — it only guards `RegisterBark` with `IsBarkRegistered()` so a
pawn respawn/relevel can never reset an already-fired bark back to unfired. This is the first
production (non-test) call site for `GetSubsystem<UGizmoNarrativeSubsystem>()` and for
`RegisterBark`/`TriggerBark` from outside the subsystem's own `Initialize()`.

## Acceptance criteria

- [x] The first successful Stun cast against an enemy triggers a "first contact" Gizmo bark via
      `UGizmoNarrativeSubsystem` (`KrowdKontrol.Unit.GizmoFirstContactComponent` case (a)).
- [x] Second and later successful Stun casts do not re-trigger it (case (b): two further
      simulated casts, `CallCount` stays at 1).
- [x] A `KrowdKontrol.Unit.` Automation Framework test
      (`KrowdKontrol.Unit.GizmoFirstContactComponent`) confirms the bark fires exactly once
      across multiple simulated Stun casts.
- [x] No changes to `UGizmoNarrativeSubsystem` or `UAbilityCastComponent` themselves — both
      already exposed everything needed (`RegisterBark`/`TriggerBark`/`IsBarkRegistered`,
      `OnAbilityCastApplied`).
- [x] `app/` edited directly, not `app-source-tracked/` (this mirror is generated at PR-creation
      time from the real `app/` edits).
- [x] Existing `KrowdKontrol.Unit.GizmoNarrativeSubsystem`, `KrowdKontrol.Unit.AbilityCastComponent`,
      and `KrowdKontrol.Unit.AbilityVFXColour` tests still pass unmodified (all 54
      `KrowdKontrol.Unit.*` tests pass, no regressions).

## Files changed

- `Source/KrowdKontrol/GizmoFirstContactComponent.h` / `.cpp` (CREATE) — the new component.
- `Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` / `.cpp` (UPDATE) — construct and bind the
  new component alongside the existing `AbilityCastVFXComponent`.
- `Source/KrowdKontrol/Private/Tests/KrowdKontrolGizmoFirstContactComponentTest.cpp` (CREATE) —
  `KrowdKontrol.Unit.GizmoFirstContactComponent`.
- `Source/KrowdKontrol/EnemyBase.h` (UPDATE, deviation from the plan) — added
  `friend class FKrowdKontrolGizmoFirstContactComponentTest;` alongside the existing friend
  grants (`FKrowdKontrolAbilityCastComponentTest`, `FKrowdKontrolAbilityVFXColourTest`, etc.),
  required because `AEnemyBase::TickCheckDetection` is private and the new test drives a real
  `AEnemyBaseTestActor` through Idle -> Alert directly, the same established pattern every other
  `AEnemyBase`-driving test already uses.

## Deviations from the plan

- **`EnemyBase.h` friend grant** (see above) — not listed in the plan's "Files to Change" table,
  but required for the plan's own Task 4 test code to compile. Confirmed via a direct
  `UnrealBuildTool` compile: fails with `C2248: 'AEnemyBase::TickCheckDetection': cannot access
  private member` without it, builds clean with it. No other change to `EnemyBase.h`.

Everything else matches the plan exactly — no other deviations.

## Validation evidence

Full gate (`python harness/ci.py`, mode=full — real `UnrealBuildTool` compile +
`KrowdKontrol.Unit.*` Automation run):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=54
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

All 54 `KrowdKontrol.Unit.*` tests pass, including the new
`KrowdKontrol.Unit.GizmoFirstContactComponent` (confirmed individually: `passed=1 total=1`). No
regressions in `KrowdKontrol.Unit.GizmoNarrativeSubsystem`, `KrowdKontrol.Unit.AbilityCastComponent`,
or `KrowdKontrol.Unit.AbilityVFXColour`. `static` stays honestly `SKIPPED` (no static-analysis
command configured yet); `HOLDOUT_ABSENT`/`MUTATIONS_ABSENT` are expected, documented bootstrap
states.

MISSION.md Hard Invariants reviewed against this diff: no-kill rule, 5-colour lock, 5-ability
roster, and 4-enemy-type roster are all untouched — the component only registers/triggers a
narrative bark line and reads the existing `EAbilitySlot::Stun` value and generic `AEnemyBase`
type. No governance files or protected paths touched.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and its matching
`app-source-tracked/` copy are the tracked-repo record of that change, per D-009. Not a substitute
for reading `app-source-tracked/` directly.
