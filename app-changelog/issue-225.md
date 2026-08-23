# Issue #225: World-space depleting duration indicator on Controlled enemies

Adds a new `UActorComponent`, `UControlledDurationIndicatorComponent`
(`app/Source/KrowdKontrol/ControlledDurationIndicatorComponent.h`/`.cpp`),
`CreateDefaultSubobject`'d once in `AEnemyBase::AEnemyBase()`
(`app/Source/KrowdKontrol/EnemyBase.cpp`) — base-class-owned, not per-subclass, since
every field it reads is already base-class-owned. It shows a small world-space plane
mesh above an `AEnemyBase`-derived enemy the instant it enters the `Controlled` state,
filled in the controlling ability's reserved colour
(`AbilityData::Get(Ability).Colour`), draining from full to empty over
`GetRemainingControlledSeconds()`/`GetTotalControlledSeconds()`, and disappearing
immediately on reversion to `Alert` or on `Banked`. Closes issue #225 (PRD
`docs/prd-enemy-effect-indicator.md`, REQ-1 and the testable half of REQ-3; REQ-2's
accessors already shipped in PR #231 / issue #224).

Driven by three explicit calls inside `AEnemyBase::ReceiveControl()` /
`TickControlledDuration()` / `TransitionToBanked()` — event-driven, not
`TickComponent()`-driven, so the indicator's reflected state is correct immediately
after those calls return with no world tick pump required, matching every existing
`EnemyBase`-family test's no-World, direct-call idiom. Per the issue's own explicit
instruction, `RefreshFillFraction()` reads the fraction exclusively through
`GetRemainingControlledSeconds()`/`GetTotalControlledSeconds()`, never the private
`RemainingControlledSeconds` field directly.

Mirrors `UAbilityTargetingIndicatorComponent`'s mesh+MID+idempotent-init+reflected-state
structure (not a reuse of that component — its documented targeting/range semantics
and one-instance-per-Owner limit make repurposing it a semantic hijack) and
`UEnemyTypeIndicatorComponent`'s attach/height-offset idiom, at a height offset
(190uu) distinct from that component's 150uu so the two read as siblings, not stacked
(REQ-3).

## Acceptance criteria

- [x] A world-space indicator appears at an enemy the moment it enters `Controlled`
      (`bIsVisible` true immediately after `ReceiveControl()`).
- [x] The indicator's fill colour is `AbilityData::Get(ControllingAbility).Colour`.
- [x] Fill fraction is `GetRemainingControlledSeconds() / GetTotalControlledSeconds()`,
      starting at 1.0 and strictly decreasing under `TickControlledDuration`.
- [x] The indicator disappears immediately on reversion to `Alert`
      (`OnEnemyControlledExpired`) or on `Banked` (`OnEnemyBanked`).
- [x] Automation test asserts all of the above, plus (per REQ-3) that the bar's
      world-space offset differs from `EnemyTypeIndicatorComponent`'s.
- [x] Zero regressions in the existing `KrowdKontrol.Unit.*` suite, specifically the
      no-`UWorld` `ReceiveControl()` callers this change's `Owner->GetWorld()` guard
      protects (`InitializeIndicatorVisual()` no-ops, retryable, when no World exists
      — same shape as the existing no-root-component branch).
- [x] No new content asset created; no `.Build.cs` change (reuses
      `/Engine/BasicShapes/Plane.Plane` and
      `/Game/_Placeholder/Abilities/M_AbilityIndicator.M_AbilityIndicator`).

New test: `KrowdKontrol.Unit.ControlledDurationIndicatorComponent`
(`app/Source/KrowdKontrol/Private/Tests/KrowdKontrolControlledDurationIndicatorComponentTest.cpp`),
cases (a)-(g): default-hidden state, immediate show at fraction 1.0 in the correct
colour, strictly-decreasing fraction under repeated ticks, hide on natural expiry,
hide on banking, the full sequence repeated on an actor with explicitly no `UWorld`
(the dominant test shape across this module — proves the `RegisterComponent()`-needs-a-World
guard), and the REQ-3 sibling-offset guard against `UEnemyTypeIndicatorComponent` on a
`CreateNewMap()`-spawned `ABomberEnemy`. Three more cases added during review self-fix:
(g2) a second `InitializeIndicatorVisual()` call on the same World-backed indicator must
not replace `FillMeshComponent` (proves the idempotent-init guard, previously
unexercised since cases (a)-(f) never reach a real `World` and (g) only initialized
once); (g3) `FillMeshComponent`'s relative X must move negative as `FillFraction` drains
below 1.0 (proves the left-anchored drain math actually anchors left, not just that
some transform changes); (h) the Sleep early-wake path
(`ReceiveControl(Sleep)` then `ReceiveControl(Root)` while still `Controlled`) must hide
the indicator on the same call that reverts the enemy to `Alert` — this is a third,
previously-untested `Hide()` call site distinct from natural expiry (d) and banking (e).

## Deviation from the plan

Review self-fix pass: corrected two doc comments that described behavior the code
doesn't actually have. `RefreshFillFraction()`'s comment claimed a "No-op (leaves
`FillFraction` unchanged)" on `GetTotalControlledSeconds() <= 0`, but the implementation
resets `FillFraction` to `0.0f` and re-applies the visual on that branch — only the
`GetOwner()`-not-an-`AEnemyBase` branch is a true no-op; the comment now says so. Two
"fields it reads" comments (on the component's class doc and
`AEnemyBase::GetControlledDurationIndicatorComponent()`) also listed `ControllingAbility`
as a field the component reads via `GetRemainingControlledSeconds()`/
`GetTotalControlledSeconds()` — it doesn't; fill colour arrives as a `Show()` parameter
sourced by the caller, not read by the component itself. Both fixed; no behavior change,
comment-only.

Beyond that, no functional deviation from the plan — one build-time fix beyond the plan's literal code blocks: the new
test's REQ-3 case needs `UTextRenderComponent`'s full type (only forward-declared in
`EnemyTypeIndicatorComponent.h`) to call `GetRelativeLocation()` on
`MarkerTextComponent`, so the test file also includes
`Components/TextRenderComponent.h` and `Components/StaticMeshComponent.h` (for the
same reason on `FillMeshComponent`). Also added an explicit
`WorldBomber->EnemyTypeIndicatorComponent->InitializeMarkerVisual()` call in that same
case — `SpawnActor()` in this test harness's `CreateNewMap()` World is never driven
through `BeginPlay()` (same limitation `KrowdKontrolEnemyBaseTest.cpp` case (t)'s
comment documents), so the marker's own `BeginPlay()`-triggered auto-init never runs
without this, exactly like `KrowdKontrolEnemyTypeIndicatorComponentTest.cpp` case (b)
already does for its own `CreateNewMap()` spawns.

## Validation evidence

```
KROWD_KONTROL_SKIP_UBT=1 python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=103
GATE_OK mode=quick
```

`KrowdKontrol.Unit.ControlledDurationIndicatorComponent` passes in isolation
(`passed=1 total=1`). The full `KrowdKontrol.Unit.*` sweep (103/103) and
`KrowdKontrol.Smoke.*` (1/1) were re-run clean multiple times, confirming zero
regressions among the no-`UWorld` `ReceiveControl()` callers this change's Task 4
GOTCHA flagged as highest-risk (`KrowdKontrolEnemyBaseTest`, `KrowdKontrolSniperEnemyTest`,
`KrowdKontrolBomberEnemyTest`, `KrowdKontrolTrooperEnemyTest`, `KrowdKontrolRunnerEnemyTest`,
and others). Two isolated single-run failures were observed and ruled out as
pre-existing, unrelated flakes, not caused by this change: one was `KrowdKontrol.Unit.LevelFailed`
failing on a `LogModelContextProtocol: Error: Call to unknown method "server/discover"`
line (a documented pre-existing MCP-connection flake that fails whatever test is
running when it fires); the other was a one-off `KrowdKontrol.Unit.EnemyBase` failure
("Enemy actor should not be destroyed by reaching Banked") on an un-rooted
`NewObject<>()` actor held only by a local pointer for the rest of that test — a risk
`KrowdKontrolEnemyBaseTest.cpp`'s own comments already document (incidental GC
collection of a heavily-exercised, never-rooted test actor) — both non-reproducible
across three immediate reruns (103/103 clean each time).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
