# Issue #245: First-entry room countdown that holds enemies Idle before activating

**Type**: enhancement (extends the merged #244/PR #274 gate)

## Summary

Today, `AEnemyBase::TickCheckDetection`'s Idle→Alert gate (issue #244) opens the
instant the player is resolved to an enemy's `OwningRoom` — enemies already in
range engage the same frame the player crosses the doorway, with zero prep window.

This adds a first-entry countdown directly on `ARoomActor`: a lightweight 0.25s
poll (mirroring `ADoorConnectorActor`'s tick-interval idiom) detects "the player's
nearest room just became this un-cleared, not-yet-activated room" and runs a
visible 3-second `"3"→"2"→"1"` countdown via the existing `UOnScreenPromptWidget`
(three chained `ShowPrompt()` calls, each ≤1.0s, so the widget's 2.0s hard cap is
never touched). While the countdown runs, `AEnemyBase::IsPlayerInOwningRoom()` —
the exact gate issue #244 added — gains one more early-out:
`Room->IsActivationPending()` (not yet activated and has an un-cleared encounter)
holds the gate closed regardless of player position. At expiry, a permanent
one-shot `bRoomActivated` latch flips and the gate's live query naturally opens on
the enemy's own next `TickCheckDetection` call — no new event/delegate/component.

As a perf follow-up in the same spirit as #244/PR #274's own review finding,
`GetCachedRoomList` (the per-frame `TActorIterator<ARoomActor>` cache) was
promoted from `EnemyBase.cpp`'s anonymous namespace to a shared
`ARoomActor::GetCachedRoomList()` static, so the new per-room 0.25s poll doesn't
reintroduce the O(rooms × enemies) per-frame world-scan that fix already closed
once for the enemy side.

## Acceptance criteria

- [x] On first entry into an un-cleared room, a visible on-screen countdown
      starts, showing 3 → 2 → 1, default 3.0s, configurable via
      `RoomActivationCountdownSeconds`.
- [x] While counting, the room's owned enemies hold Idle regardless of player
      position (verified at zero distance in the new test).
- [x] At expiry, the gate opens and owned enemies run normal detection
      (verified: `TickCheckDetection` reaches `Alert` immediately after
      `ActivateRoom()`).
- [x] Fires exactly once per room per run — re-entering an already-cleared or
      already-activated room never re-triggers it.
- [x] `Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActivationCountdownTest.cpp`
      exists in both `app/` and `app-source-tracked/`, passes, and covers all 4
      issue ACs plus the widget-text assertion.
- [x] `KrowdKontrol.Unit.EnemyRoomDetectionGate` (issue #244's test) still passes
      unmodified — no regression to the underlying gate.
- [x] `UOnScreenPromptWidget::MaxPromptDurationSeconds` is untouched (still
      2.0f) — PRD 09 REQ-4's hard cap is not weakened.

## Deviations from the plan

**`IsActivationPending()`'s definition.** The plan's literal proposed code
(`!bRoomActivated && !IsRoomCleared()`) permanently blocks Idle→Alert for any
room with unbanked owned enemies until the countdown machinery actually runs to
completion — but pre-existing tests (`KrowdKontrolEnemyRoomDetectionGateTest.cpp`,
`KrowdKontrolRoomActorDoorGatingTest.cpp`, `RoomActorRemainingEnemyCount`,
`QuestTrackerWidgetRoomState`) drive Idle→Alert via direct `TickCheckDetection()`
calls without ever starting a countdown, so under that definition those rooms
would never activate — a real regression confirmed by running the suite (all 4
failed). Implemented as `return bCountdownActive;` instead, matching the issue's
AC wording precisely ("**While counting**... hold Idle") — the gate now closes
only for the countdown's actual running duration, not the entire pre-first-entry
period. All 4 tests pass again with this definition; full `KrowdKontrol.Unit.`
suite is green (`passed=102 total=102`). See `implementation.md` for the full
trade-off analysis (a theoretical <0.25s race window on the very first tick after
entry, same order of imprecision the plan's own Risks section already accepts for
the countdown digit display).

`docs/prd-room-encounter-flow.md` REQ-3's `— ✅ implemented, PR #N` marker is
**not** added in this commit. The real precedent for this exact PRD (REQ-2, this
same file) shows that marker was only added in a follow-up commit once the PR
number actually existed (`4c05ac0`, "fix: address review findings", added after
PR #274 was open) — not during the initial implement pass, which has no PR number
to reference yet. Left for a follow-up pass once this issue's PR number is known.

## Validation evidence

See `validation.md` (written by the separate `dark-factory-validate` node).

## Files

| File | Action |
|------|--------|
| `app/Source/KrowdKontrol/RoomActor.h` | UPDATE |
| `app/Source/KrowdKontrol/RoomActor.cpp` | UPDATE |
| `app/Source/KrowdKontrol/EnemyBase.h` | UPDATE |
| `app/Source/KrowdKontrol/EnemyBase.cpp` | UPDATE |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolRoomActivationCountdownTest.cpp` | CREATE |

## Out of scope (per the issue and PRD's own Out of Scope)

- De-aggro/leashing — untouched; this only gates Idle→Alert onset.
- Line-of-sight/perception systems — room membership via `FindNearestRoom` stays
  the only scoping mechanism.
- Any audio/visual polish beyond the readable "3"/"2"/"1" digits.
- Wave-spawner-triggered re-countdowns — `bRoomActivated` is a permanent latch;
  enemies added via `AddOwnedEnemy()` after activation immediately follow the
  unmodified REQ-2 gate.
