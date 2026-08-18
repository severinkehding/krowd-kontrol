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
- [x] A pawn respawn/relevel (a fresh `UGizmoFirstContactComponent` instance sharing the same,
      persistent `UGizmoNarrativeSubsystem`) cannot reset an already-fired bark back to unfired
      (case (c) — added during review; closes a gap where `RegisterBark`'s overwrite semantics
      were documented but never actually exercised by a test).
- [x] A non-Stun ability cast never fires or registers the bark (case (d)).
- [x] A missing `GameInstance`/unresolvable subsystem degrades safely — no crash (case (e), added
      during review).
- [x] The pawn's real constructor-time `AddDynamic` binding (not just a test-constructed
      stand-in) actually reaches `GizmoFirstContactComponent` (case (f), added during review).
- [x] `InitializeFirstContactBark()`'s guard flag is only latched after `ResolveNarrativeSubsystem()`
      confirms success, matching `UAbilityCastVFXComponent::InitializeCastVFX()`'s and
      `UEnemyTypeIndicatorComponent::InitializeMarkerVisual()`'s established retry-on-failure idiom
      (bug found and fixed during review — the original code latched the guard unconditionally,
      permanently disabling the documented retry path on a transient resolution failure).
- [x] A `KrowdKontrol.Unit.` Automation Framework test
      (`KrowdKontrol.Unit.GizmoFirstContactComponent`) confirms the bark fires exactly once
      across multiple simulated Stun casts, and now also covers the respawn-safety, missing-
      subsystem, and real-pawn-wiring cases above.
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
- `Source/KrowdKontrol/EnemyBase.h` (UPDATE, deviation from the plan) — added **two** new friend
  grants, not one: `friend class FKrowdKontrolGizmoFirstContactComponentTest;` (this PR's own
  test) and `friend class FKrowdKontrolAbilityVFXColourTest;`. Neither predates this PR — `main`'s
  `EnemyBase.h` had neither. See "Deviations from the plan" below for why the second grant is in
  this diff at all.
- `Source/KrowdKontrol/AbilityCastVFXComponent.h` / `.cpp` (CREATE in `app-source-tracked/` only —
  unmodified, pre-existing in `app/` since issue #67) and
  `Source/KrowdKontrol/Private/Tests/KrowdKontrolAbilityVFXColourTest.cpp` (CREATE in
  `app-source-tracked/` only, same reason) — backfilled into the tracked mirror during review, see
  "Deviations from the plan" below.

## Deviations from the plan

- **`EnemyBase.h`'s `FKrowdKontrolGizmoFirstContactComponentTest` friend grant** — not listed in
  the plan's "Files to Change" table, but required for the plan's own Task 4 test code to compile.
  Confirmed via a direct `UnrealBuildTool` compile: fails with `C2248:
  'AEnemyBase::TickCheckDetection': cannot access private member` without it, builds clean with
  it.
- **`EnemyBase.h`'s `FKrowdKontrolAbilityVFXColourTest` friend grant** — also new in this PR's
  diff, and initially undocumented (review finding). It is **not** required by anything in this
  PR's own scope — it's required by issue #67's `KrowdKontrolAbilityVFXColourTest.cpp`, which
  calls `Enemy->TickCheckDetection(...)` (private) and was already live and passing against `app/`
  before this PR (hence "no regressions in `KrowdKontrol.Unit.AbilityVFXColour`" below is accurate
  against the real working copy). That test's own `.h`/`.cpp`/test-file trio was implemented
  directly in `app/` for issue #67 and never went through `create-pr`'s `app-source-tracked/`
  mirroring step, so this PR — being both the first to touch `EnemyBase.h`'s friend list since and
  the first to add tracked source (`FlatCamera3DPrototypePawn`'s wiring) that structurally
  references `UAbilityCastVFXComponent` — inherited the gap and is where it became visible. Fixed
  during review: `AbilityCastVFXComponent.h/.cpp` and `KrowdKontrolAbilityVFXColourTest.cpp` are
  now backfilled into `app-source-tracked/` (unmodified copies of the real, already-working `app/`
  files), so both friend grants are now self-explanatory from the tracked repo alone.

Everything else matches the plan exactly.

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
