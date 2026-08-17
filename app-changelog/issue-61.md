# Issue #61: Milestone-triggered Gizmo story beats (TriggerBarkForMilestone)

Extends `UGizmoNarrativeSubsystem` (issue #57) with a milestone-shaped entry point
that a future level/progression system can call when a story beat is reached, plus
placeholder bark content for all six narrative-arc beats.

## Files changed

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `GizmoNarrativeSubsystem.h` | UPDATE | Declares `Initialize()` override, `RegisterPlaceholderMilestoneBarks()` (idempotent, guarded by `bHasRegisteredPlaceholderMilestoneBarks`), `TriggerBarkForMilestone(FName)` |
| `GizmoNarrativeSubsystem.cpp` | UPDATE | Implements the three new methods + six placeholder bark registrations |
| `Private/Tests/KrowdKontrolGizmoNarrativeSubsystemTest.cpp` | UPDATE | New milestone-trigger test section (5), plus an unknown-milestone-tag assertion and stale-listener cleanup before the section runs |

## Acceptance criteria

- [x] **Milestone-tag entry point exists.** `TriggerBarkForMilestone(FName)` forwards
      directly to `TriggerBark()`, inheriting its once-only-fire and unknown-ID-no-op
      guarantees unmodified.
- [x] **Placeholder content for all six narrative-arc beats.** Five story beats (Meet
      Krowd, Saving Fellow Robots, Asleep for a Long Time, Hidden Enemy Revealed,
      Final Chapter) plus the distinct Krowd age-reveal beat (age 203), registered via
      `RegisterPlaceholderMilestoneBarks()`.
- [x] **Automatic registration for real engine-booted instances.** New
      `Initialize(FSubsystemCollectionBase&)` override calls `Super::Initialize()`
      then `RegisterPlaceholderMilestoneBarks()`.
- [x] **Idempotent registration.** `RegisterPlaceholderMilestoneBarks()` is guarded by
      `bHasRegisteredPlaceholderMilestoneBarks`, matching
      `URoomEnemyBudgetController::InitializeRoom()`'s idempotency precedent in full
      (a second call is a safe no-op rather than silently un-firing already-fired
      barks).
- [x] **Test coverage.** `KrowdKontrol.Unit.GizmoNarrativeSubsystem` extended with new
      assertions: once-only firing per milestone tag, independent triggering of all
      five story beats, the age-reveal entry's "203" content, and an unregistered
      milestone tag's no-op-with-warning behavior.
- [x] **No real call site wired.** Explicitly out of scope per the issue - only the
      unit test calls `TriggerBarkForMilestone`; real level/progression wiring depends
      on PRD 05 (not yet built).

## Validation

`harness/ci.py` full mode - `GATE_OK`:

```
UNIT_PASSED tests=42
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
GATE_OK mode=full
```

## Review follow-up (this pass)

Addressed findings from the automated review round:

- Added the `bHasRegisteredPlaceholderMilestoneBarks` idempotency guard
  (code-review Finding 1 / comment-quality Finding 1) so
  `RegisterPlaceholderMilestoneBarks()` actually matches the `InitializeRoom()`
  precedent its own comment cites, instead of only half-applying it.
- Updated the class-level doc comment to stop claiming "no bark content... here"
  now that placeholder content lives in this file (comment-quality Finding 2).
- Added a comment to the new `Initialize()` override documenting the
  `Super::Initialize()`-first ordering requirement (comment-quality Finding 3).
- Unbound the earlier test sections' listeners before section (5) so milestone
  triggers can't incidentally re-invoke stale reentrant test listeners
  (test-coverage Finding 2).
- Added a direct `TriggerBarkForMilestone` unknown-tag assertion as a regression
  pin (test-coverage Finding 3).
- This file itself was added to close the missing-changelog gap flagged by the
  docs-impact review.
