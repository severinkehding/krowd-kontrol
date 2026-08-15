# Issue #81: Add IThreatState interface exposing hot/idle signal for loop legibility

Adds a minimal, self-contained C++ data contract to the `KrowdKontrol` module: an
`EThreatState` enum (`Idle`, `Hot`) and an `IThreatState` `UINTERFACE` exposing
`EThreatState GetThreatState() const`. This closes the gap PRD 01 REQ-1 depends on
("player must always be able to tell, at a glance, which enemies are hot vs idle") —
no enemy AI state machine exists yet in this repo, so this issue only establishes the
contract future systems (enemy AI writing state, future HUD reading it) will use. Data
contract only: no AI, animation, or HUD rendering logic included.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/ThreatState.h` | CREATE | `EThreatState` enum (`Idle`, `Hot`, `BlueprintType`) + `IThreatState` `UINTERFACE(MinimalAPI)` exposing pure-virtual `GetThreatState() const`. Header-only (no `.cpp` — nothing to implement). Lives at module root, not `Public/`, matching this module's actual layout (`KrowdKontrol.Build.cs`, no `Public/` folder exists). |
| `app/Source/KrowdKontrol/Private/Tests/ThreatStateTestActor.h`/`.cpp` | CREATE | Minimal test-only `AActor` implementing `IThreatState`, with a `SetThreatState()` setter for the test to drive. Mirrors the `RoomClearedTestListener` test-helper pattern — needs its own header because `UCLASS` is UHT-parsed from headers only. |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolThreatStateTest.cpp` | CREATE | `KrowdKontrol.Unit.ThreatState` — constructs the test actor via `NewObject` (no `UWorld` needed, pure accessor), `Cast<IThreatState>()`s it to prove the interface dispatches polymorphically (not just through the concrete type), toggles state Idle→Hot→Idle, asserts `GetThreatState()` reports correctly at each step through both the concrete actor and the interface pointer. |

No `.Build.cs` change needed — `UObject/Interface.h` ships with the module's existing
`CoreUObject` dependency, and this test needs no `UWorld`/`UnrealEd`.

## Acceptance criteria

- [x] **`EThreatState`/`IThreatState` compile in their own header under
      `Source/KrowdKontrol/`.** Confirmed via `harness/ci.py` full-mode gate (module
      builds successfully as part of `UE_AUTOMATION_OK`).
- [x] **`KrowdKontrol.Unit.ThreatState` Automation Framework test exists**, provides a
      minimal test-only actor implementing `IThreatState`, toggles its underlying
      state, and confirms `GetThreatState()` reports `Hot`/`Idle` correctly. Covers
      default state (`Idle`), toggle to `Hot`, and toggle back to `Idle`, and asserts
      through a `Cast<IThreatState>()`-obtained interface pointer (not just the
      concrete actor type) to prove polymorphic dispatch actually works.
- [x] **No AI, animation, or HUD rendering logic included anywhere in this change.**
      Reviewed by inspection — pure enum/interface data contract only.

## Validation

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=3
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`UNIT_PASSED tests=3` covers the new `KrowdKontrol.Unit.ThreatState` test alongside the
two pre-existing `KrowdKontrol.Unit.*` tests — no regression. MISSION.md hard
invariants reviewed by inspection: no governance files touched, no kill logic (the
enum is a two-value hot/idle signal, not a health/death mechanic), no colour usage, no
ability or enemy type added, no engine/dimensionality/networking change, Unreal
project remains untracked in git.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
