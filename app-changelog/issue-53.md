# Issue #53: Enforce ability-gating in the room-pool shuffler

Closes the gap `URoomPoolShufflerComponent` left open when it landed (issue #51):
`ShuffleRooms()` filtered by `ERoomDifficultyTier` only and never read
`URoomMetadataComponent::RequiredAbility`, despite PRD `05-level-design-and-
progression.md` REQ-5 requiring ability-gated rooms to be excluded until the player has
unlocked the required ability. Adds a fourth `ShuffleRooms()` parameter, `const
UAbilityUnlockComponent* UnlockState`, and extends the existing per-room filter loop to
also require `UnlockState->IsAbilityUnlocked(...)` (mapped from `ERoomAbilityGate` via a
documented `-1` offset) for any room with a non-`None` `RequiredAbility`. Mirrors the
pointer-consumer pattern `UAbilityCooldownTrayWidget::BindAbilityUnlockComponent()`
already established for the same `UAbilityUnlockComponent` class: null `UnlockState` is
fail-closed (every gated room excluded, logged warning), not a silent skip.

## Files changed

All paths are under `app/` (gitignored per D-003) — this table and the matching
`app-source-tracked/` copy are the tracked-repo record of that change; see the closing
note below.

| File | Action | What changed |
|------|--------|----------------|
| `app/Source/KrowdKontrol/RoomPoolShufflerComponent.h` | UPDATE | Forward-declares `UAbilityUnlockComponent`; adds `UnlockState` as `ShuffleRooms()`'s 4th parameter; class/function comments updated to describe the new gating behavior (removes the now-false "REQ-5 out of scope" claim) |
| `app/Source/KrowdKontrol/RoomPoolShufflerComponent.cpp` | UPDATE | Includes `AbilityUnlockComponent.h`; logs (does not early-return on) a null `UnlockState`; extends the tier-match filter branch with an `ERoomAbilityGate`→`EAbilitySlot` gate check, fail-closed on null/locked |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomPoolShufflerComponentTest.cpp` | UPDATE | Passes a constructed `DefaultUnlockState` as the 4th arg to all 5 pre-existing `ShuffleRooms()` calls (signature change); appends section (h) with 3 new assertions covering locked-excludes, unlocked-includes, and null-fails-closed |

## Acceptance criteria

- [x] **`ShuffleRooms()` excludes any room whose `RequiredAbility` names an ability
      `UnlockState` does not report as unlocked.** Test case (h-a): a Root-gated room is
      excluded from a 2-room pool while `GatingUnlockState` reports Root locked.
- [x] **`KrowdKontrol.Unit.RoomPoolShufflerComponent` covers both exclusion while locked
      and inclusion once unlocked.** Test case (h-a) covers exclusion; (h-b) calls
      `NotifyLevelReached(3)` (unlocks Root) and confirms both rooms come back.
- [x] **No new abilities/enum values added.** `EAbilitySlot`/`ERoomAbilityGate` are
      unchanged; the gate check reuses the existing 5-slot ordering via a single
      commented `-1` offset cast.
- [x] **All pre-existing assertions (tier-filtering, door-chain, reproducibility,
      different-seed ordering, zero-match, single-match) still pass unmodified.**
      Sections (a)-(g) only gained the new 4th `ShuffleRooms()` argument, no behavioral
      change — confirmed by the full-suite run below.
- [x] **Fail-closed on null `UnlockState`.** Test case (h-c): passing `nullptr` still
      excludes the Root-gated room, even with nothing else claiming Root is locked.

## Validation

```
$ python harness/ci.py         # full mode
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=58
APP_STARTED driver=cli
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`tests=58` (up from 51 at issue #51) includes the extended
`KrowdKontrol.Unit.RoomPoolShufflerComponent` test with its 3 new gating assertions, all
passing. No regressions in any pre-existing `KrowdKontrol.Unit.*` test. Hard Invariant #4
(exactly 5 abilities) untouched — reviewed by inspection, no new enum values added.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and its
matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
