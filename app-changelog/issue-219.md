# Issue #219: Level-1 core-loop instruction prompts

## Summary

Adds `UTeachingPromptComponent`, a new `UActorComponent` attached to
`AFlatCamera3DPrototypePawn` alongside its other `OnAbilityCastApplied` subscribers.
Shows four one-shot on-screen instruction prompts during Level 1 only - "STUN IT —
PRESS 1", "IT FOLLOWS YOU — WALK", "DROP IT ON THE GLOWING PEN", "ROOM CLEAR — DOOR
OPEN" - each tied to a real gameplay signal already wired elsewhere in this module
(ability casts, `EThreatState::Hot`, `ATargetZone` proximity, `AEnemyBase::OnEnemyBanked`,
`ARoomActor::OnRoomClearedStateChanged`). This is REQ-3's core-loop half; the
ability-unlock half already shipped as `UAbilityUnlockPromptComponent` (issue #220).

No new UI - every prompt reuses `UOnScreenPromptWidget::ShowPrompt()`, the same surface
`UAbilityMatchupNudgeComponent`/`UAbilityUnlockPromptComponent` already drive. No new
delegate anywhere: the "first hot enemy" and "controlled enemy near a target zone"
conditions are polled from `TickComponent()`, mirroring `UMusicSubsystem::IsAnyEnemyInCombat()`'s
and `UFirstStunBeaconComponent::FindNearestTargetZone()`'s existing scan shapes,
respectively - no new `AEnemyBase`/`ADoorConnectorActor` delegate was added, per the
plan's Design Decisions.

One component owns all four prompts' state (not four separate components, unlike this
module's usual one-component-per-behavior precedent) because prompts 2 and 3 both need
the same tracked "first controlled enemy" reference, which a four-component split would
otherwise have to thread via a new cross-component signal.

## Files Changed

| File | Action |
|---|---|
| `Source/KrowdKontrol/TeachingPromptComponent.h` | CREATE |
| `Source/KrowdKontrol/TeachingPromptComponent.cpp` | CREATE |
| `Source/KrowdKontrol/FlatCamera3DPrototypePawn.h` | UPDATE - forward-declare + `TeachingPromptComponent` property |
| `Source/KrowdKontrol/FlatCamera3DPrototypePawn.cpp` | UPDATE - construct + wire to `AbilityCastComponent->OnAbilityCastApplied` |
| `Source/KrowdKontrol/EnemyBase.h` | UPDATE - `friend class FKrowdKontrolTeachingPromptComponentTest;` |
| `Source/KrowdKontrol/Private/Tests/KrowdKontrolTeachingPromptComponentTest.cpp` | CREATE |

## Acceptance Criteria

| Criterion | Status |
|---|---|
| "STUN IT — PRESS 1" fires on the first enemy in Level 1 to reach `EThreatState::Hot`, dismissed by the player's first successful Stun cast | Done — case (a) |
| "IT FOLLOWS YOU — WALK" fires on the first successful ability cast that applies control to any enemy, dismissed by player movement while that specific enemy remains Controlled | Done — cases (b)/(c) |
| "DROP IT ON THE GLOWING PEN" fires when the tracked first-controlled enemy comes within `DropZoneProximityUnits` of a real `ATargetZone`, dismissed when that specific enemy reaches Banked | Done — case (d), including the negative "a different enemy banking must not dismiss it" case |
| "ROOM CLEAR — DOOR OPEN" fires once the first non-empty room in Level 1 becomes cleared, ignoring vacuous empty rooms | Done — case (e) |
| Only Level 1 (per `ParseLevelIndexFromMapName`) ever shows any of these four prompts | Done — case (f); every other case's implicit coverage since `CreateNewMap()`'s synthetic map name also defaults to level 1 |
| Each of the four fires at most once per pawn/component instance and never re-fires after its guard is set | Done — case (g), representative for the stun prompt; `CheckStunPromptFireCondition()`/`CheckDropPromptFireCondition()` both carry an internal fire-once guard so this holds even when called directly, not just through `TickComponent()`'s own gating |
| Automation test covers all acceptance-criteria bullets | Done — cases (a)-(g) in `KrowdKontrol.Unit.TeachingPromptComponent` |
| No existing test regresses | `AbilityMatchupNudgeComponent`, `AbilityUnlockPromptComponent`, `RoomActorDoorGating`, `FirstStunBeaconComponent`, `GizmoFirstContactComponent` untouched by this change |

## Deviations from Plan

- Added an internal `if (bHasFiredStunPrompt) return;` guard to `CheckStunPromptFireCondition()`
  and `if (bHasFiredDropPrompt || ...) return;` to `CheckDropPromptFireCondition()`. The
  plan's pseudocode relied solely on `TickComponent()`'s own caller-side guard
  (`if (!bHasFiredStunPrompt) CheckStunPromptFireCondition();`), which is correct for the
  real per-frame path but leaves the "fires exactly once" contract non-robust to a test
  (or any future caller) invoking the `Check*` method directly a second time. The added
  guards make the fire-once contract self-enforcing regardless of caller, matching the
  intent of AC "never re-fires after its guard is set" and test case (g)'s design.
- Test case (c) uses the real `AFlatCamera3DPrototypePawn`'s own constructor-created
  `TeachingPromptComponent` (`Pawn->TeachingPromptComponent`) rather than constructing a
  second, separate `UTeachingPromptComponent` via `NewObject` as the plan's prose
  literally suggested - the pawn's constructor already creates and owns exactly one
  instance (this same issue's Task 4), so a second instance would be redundant and
  untested-in-context. Friend access and the manual `BeginPlay()` call work identically
  either way.

## Validation Evidence

See `implementation.md` and `validation.md` in the workflow run artifacts for the full
record.
