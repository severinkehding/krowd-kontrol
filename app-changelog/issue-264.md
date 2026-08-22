# Issue #264: Reusable ability targeting-indicator component

## Summary

Adds `UAbilityTargetingIndicatorComponent`, a new `UActorComponent` that renders a
placeholder-quality translucent ground shape (circle-at-actor, circle-at-cursor, cone,
line) in an ability's locked `AbilityData` colour, with a `Show`/`Hide`/`Flash` API.
This is the rendering primitive REQ-3 of the Cursor & Aiming Foundation PRD
(`docs/prd-cursor-aiming.md`) calls for; every ability in the follow-on Ability
Targeting Shapes PRD will drive an instance of it instead of reimplementing shape
rendering per-ability.

The component owns exactly one free-floating (never-attached) `UStaticMeshComponent`
(an engine `Plane` mesh) plus one `UMaterialInstanceDynamic`, mirroring
`UAbilityCastVFXComponent`'s "world-fixed, not owner-relative" idiom and its
`FTimerHandle`-based flash-then-clear shape. One placeholder material covers all four
shape kinds via a polar angle+radius mask (`ConeHalfAngleDegrees` material param) —
circle = 180°, cone = half the caller's full angle, line = a small fixed angle — so no
`ProceduralMeshComponent` dependency and no per-shape mesh switching.

Input/cast wiring onto `AFlatCamera3DPrototypePawn` (press/hold semantics, cooldown
gating) is explicitly out of scope — that is the separate "Wire press/hold indicator
semantics to all five ability keys" issue. This issue only builds and tests the
primitive itself.

## Acceptance Criteria

| Criterion | Status |
|---|---|
| Component accepts a shape spec parameterized by kind (circle-at-actor, circle-at-cursor, cone, line) plus radius/angle/length/origin parameters | Done — `FAbilityIndicatorShapeSpec` |
| Shape renders as a translucent ground primitive in the calling ability's `AbilityData` reserved colour | **Degraded** — see Known Gaps below. Component state (colour/shape/visibility) is fully correct and tested regardless; the placeholder `M_AbilityIndicator` material asset itself could not be authored in this pass |
| `Show`/`Hide`/`Flash` API exists and is usable by future ability-cast code without touching rendering details | Done |
| Automation test: given a shape spec and an `AbilityData` colour, the indicator's colour state matches that `AbilityData` entry, and switching shape kinds produces the corresponding geometry/parameters | Done — `KrowdKontrol.Unit.AbilityTargetingIndicatorComponent` cases (b)/(c) |
| No `AFlatCamera3DPrototypePawn` wiring, no input changes | Done — component is not referenced by any pawn/input code |
| `app/` and `app-source-tracked/` copies of all changed/new files are identical | Done — verified via `diff`, no output |
| `docs/prd-cursor-aiming.md` REQ-3 marked implemented; changelog exists with a full acceptance-criteria mapping | Done — REQ-3 heading updated with an explicit "input wiring still open" caveat, matching the actual scope of this issue |
| No regressions in existing automation tests | New test file only; no existing file's production logic was touched |
| `python harness/ci.py` reports `GATE_OK` | See Validation Evidence |

## Known Gaps

**Task 1 (placeholder material `Content/_Placeholder/Abilities/M_AbilityIndicator`) is
degraded, stated plainly per MISSION.md's "loud, never silent" principle.** This pass
launched the real Unreal Editor (`scripts/ue_editor_launch_and_wait.sh`) and confirmed
it came up with its MCP HTTP endpoint responding (`UE_EDITOR_READY mcp_http_code=405`),
but the `unreal-mcp` MCP client in this session never established a session-level
connection to it (no `mcp__unreal-mcp__*` tools ever became available, despite the
underlying HTTP endpoint being reachable) — so the material asset could not actually be
authored this pass. This is a different failure mode than the previously-documented
`project_factory_worktree_no_unreal_mcp_network_path` gap (that one is a raw
socket-level failure; here the socket-level endpoint was reachable, but the MCP
client's own session was never established/retried) but the practical outcome is the
same.

Per this issue's own plan (Task 1's GOTCHA), the component code guards against exactly
this: `AbilityTargetingIndicatorComponent.cpp`'s `InitializeIndicatorVisual()` loads both
the plane mesh and the material via `TSoftObjectPtr::LoadSynchronous()` with a
null-pointer check on each result (not `ConstructorHelpers::FObjectFinder` — that was
tried first and crashed the Editor outright, since it's constructor-only and this call
site is deliberately deferred out of the constructor until an Owner with a root component
exists), so a missing material degrades to "no material set on
`IndicatorMeshComponent`, component state still fully correct and testable" rather than
a compile or runtime failure. Each failed load also now emits a `UE_LOG(LogTemp, Warning, ...)`
line naming the missing asset and its consequence, so the degraded state is loud, not
silent, in the Output Log. All acceptance criteria that don't depend on the actual
rendered visual are met and automation-tested; a human (or a future pass with a working
MCP session) still needs to author the `M_AbilityIndicator` material asset per Task 1's
spec (Surface/Unlit/Translucent, `Colour`/`Opacity`/`ConeHalfAngleDegrees` params) for
the indicator to actually render anything visible in PIE.

## Validation Evidence

See `implementation.md` and `validation.md` in the workflow run artifacts for the full
record.
