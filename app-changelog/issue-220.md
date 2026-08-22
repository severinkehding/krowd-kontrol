# Issue #220: Per-level ability-unlock instruction prompt

## Summary

Adds `UAbilityUnlockPromptComponent`, a new `UActorComponent` that binds to the
already-existing `UAbilityUnlockComponent::OnAbilityUnlocked` delegate and, on each
broadcast, shows a one-time `UOnScreenPromptWidget::ShowPrompt()` call naming the
newly unlocked ability, its key, and its colour-matched countered enemy type (e.g.
"SLEEP — PRESS 2 — STRONG VS SNIPERS" for Sleep/level 2). Wired into
`AFlatCamera3DPrototypePawn`'s constructor the same way `UAbilityMatchupNudgeComponent`
(issue #40) is wired to `UAbilityMatchupSignalComponent`.

No new UI, no new unlock logic, and no new mapping — this is a thin consumer of two
already-merged, already-tested primitives (`UAbilityUnlockComponent` and
`UOnScreenPromptWidget`). Fire-once-per-ability-per-run needs no new guard: it falls
straight out of `UAbilityUnlockComponent::UnlockAbility()`'s existing idempotency
guarantee that `OnAbilityUnlocked` broadcasts at most once per ability per component
instance.

## Acceptance Criteria

| Criterion | Status |
|---|---|
| Reuses `UOnScreenPromptWidget` — no new UI framework introduced | Done |
| On each of levels 2-5, exactly one correctly-worded prompt fires | Done — covered by `KrowdKontrol.Unit.AbilityUnlockPromptComponent` case (a) |
| Each prompt fires at most once per ability per run, no re-fire on a repeat `NotifyLevelReached` | Done — case (b) exercises the integration end-to-end; the underlying guarantee that `OnAbilityUnlocked` fires at most once per ability is proven directly against `UAbilityUnlockComponent` in `KrowdKontrolAbilityUnlockSequenceTest.cpp` case (f) |
| Automation test covers all acceptance-criteria bullets | Done — cases (a)-(d), including missing-widget defensive behavior and real-pawn constructor wiring |
| No regressions in existing tests | `AbilityUnlockSequence`, `AbilityMatchupNudgeComponent`, `AbilityUnlockLevelSubsystem` untouched by this change |
| Code mirrors `AbilityMatchupNudgeComponent`'s existing patterns | Done — same lazy prompt-widget resolution, one-shot warning, `AddDynamic` wiring shape |

## Validation Evidence

See `implementation.md` and `validation.md` in the workflow run artifacts for the full
record.
