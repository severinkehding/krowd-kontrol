# Issue #359: Add visible targeting telegraph when SN-1PR sniper acquires the player

## Summary

Adds a world-space "shot incoming" telegraph — a thin ground line from the SN-1PR
sniper (`ASniperEnemy`) to the player — that appears the instant the sniper enters
Attack against the player and disappears the moment that threat resolves (shot
fired, sniper Controlled by any ability, or the player breaks attack range). It
reuses the existing `UAbilityTargetingIndicatorComponent`'s `Line` shape kind, the
same one `UAbilityPressHoldComponent` already uses for the player's own
cursor-aim line, and the sniper's existing `AttackTellLightComponent` colour, so
no new rendering component or colour is introduced. The telegraph is layered
alongside (never replacing) the sniper's existing audio/light attack tell.

## Acceptance Criteria Checklist

- [x] Spawns/activates on Attack acquisition — `TelegraphIndicatorComponent`
      shows a `Line` shape from `OnAttackEntry()`, refreshed every `Tick()` while
      genuinely in Attack (before the shot fires) so it tracks the player's live
      position.
- [x] Non-reserved colour (MISSION.md Hard Invariant #3, the 5-colour lock) —
      reuses `AttackTellLightComponent->GetLightColor()` directly rather than a
      new literal; test case (z) asserts both direct equality and non-collision
      against `ReservedGameplayColours::GetAll()`.
- [x] Layered alongside, not replacing, the existing audio tell —
      `AttackTellSound`/`AttackTellAudioComponent` logic is untouched.
- [x] Deactivates on shot-fire — `Hide()` called in `AdvanceAttackTelegraph()`'s
      shot-fire branch, alongside the `OnSniperShotFired` broadcast.
- [x] Deactivates on Controlled by any ability, including Root (the one ability
      that does *not* clear the on-body light tell) — `OnControlledEntry()` calls
      `Hide()` unconditionally, deliberately not gated on
      `bAllowsAttackWhileControlled` the way the light tell is. Test case (z2)
      exercises the Root case specifically to prove this divergence.
- [x] Deactivates when the target clears (player leaves attack range) — reuses
      the existing `OnAttackExpired()` hook, which issue #360 (merged, PR #387)
      already guarantees fires on both the timeout and range-break exits, so no
      new signal was added. Extended case (v) with a `bIsVisible` assertion.
- [x] Unit test coverage of the component's reflected state (`bIsVisible`,
      `CurrentShapeSpec.Kind`, `CurrentColour`) at each transition above.

## Validation Evidence

`harness/ci.py` full mode: `UNIT_PASSED tests=127`, `PIE_PASSED tests=8`,
`UE_BUILD_OK`, `UE_AUTOMATION_RESULT passed=1 total=1`, `GATE_OK mode=full`. No
regressions in existing `KrowdKontrolSniperEnemyTest.cpp` cases (a-y). `app/` and
`app-source-tracked/` confirmed byte-identical for all 3 changed files, with no
concurrent-task leakage from open PR #385 (issue #358, also touches
`SniperEnemy.{h,cpp}`) — independently re-verified at PR-creation time by diffing
this commit against `origin/main`.
