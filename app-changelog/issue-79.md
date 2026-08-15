# Issue #79: Add IHerdable interface marking actors as under crowd-control

Adds a minimal, self-contained C++ data contract to the `KrowdKontrol` module: a
`UHerdable`/`IHerdable` `UINTERFACE` exposing `bool IsControlled() const` (true while an
actor is under an active CC effect) and `FName GetHerdColourTag() const` (which of the
five locked colour categories the actor currently matches). This closes the gap PRD 01
REQ-2 depends on — a future `ATargetZone` actor (a separate, not-yet-built issue) needs a
type-agnostic way to ask "are you controlled, and what colour are you" without coupling
to concrete enemy classes. Data contract only: no CC-effect implementation, enemy AI, or
colour-rendering logic included.

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/Herdable.h` | CREATE | `UHerdable` `UINTERFACE(MinimalAPI)` + `KROWDKONTROL_API IHerdable` exposing pure-virtual `IsControlled() const` and `GetHerdColourTag() const`. Header-only (no `.cpp`). Lives at module root, not `Public/`, matching this module's actual layout (mirrors `ThreatState.h`, issue #81). |
| `app/Source/KrowdKontrol/Private/Tests/HerdableTestActor.h`/`.cpp` | CREATE | Minimal test-only `AActor` implementing `IHerdable`, with `SetControlled()`/`SetHerdColourTag()` setters for the test to drive. Mirrors the `ThreatStateTestActor` pattern. |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolHerdableTest.cpp` | CREATE | `KrowdKontrol.Unit.Herdable` — constructs the test actor via `NewObject` (no `UWorld` needed, pure accessors), confirms `Implements<UHerdable>()` and `Cast<IHerdable>()` both work, toggles controlled state and colour tag, asserts through both the concrete actor and the interface pointer. |

No `.Build.cs` change needed — `UObject/Interface.h` (CoreUObject) and `FName` (Core) are
already covered by the module's existing `PublicDependencyModuleNames`.

## Acceptance criteria

- [x] **`IHerdable` compiles as a standard Unreal `UINTERFACE`, declared at
      `Source/KrowdKontrol/Herdable.h`** (module root, not the issue body's literal
      `Public/` wording — follows `KrowdKontrol.Build.cs`'s documented, already-working
      convention per `CLAUDE.md`'s "code convention wins on code style" precedence).
      Confirmed via `harness/ci.py` full-mode gate (module builds successfully as part
      of `UE_AUTOMATION_OK`).
- [x] **`KrowdKontrol.Unit.Herdable` Automation Framework test exists**, provides a
      minimal test-only actor implementing `IHerdable`, and confirms the interface is
      queryable via both `Cast<IHerdable>()` and `Implements<UHerdable>()`. Covers
      default state (`IsControlled() == false`, `GetHerdColourTag() == NAME_None`),
      toggle to controlled/`Purple`, and toggle back, asserting through both the
      concrete actor and the interface pointer to prove polymorphic dispatch works.
- [x] **No enemy AI, ability, or colour-rendering logic added anywhere in this
      change.** Reviewed by inspection — pure interface data contract only.

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

`UE_AUTOMATION_RESULT passed=1 total=1` covers the new `KrowdKontrol.Unit.Herdable`
test; no regression in the pre-existing `KrowdKontrol.Unit.*` suite. MISSION.md hard
invariants reviewed by inspection: no governance files touched, no kill logic, no
colour-rendering logic, no ability or enemy type added, no engine/dimensionality/
networking change, Unreal project remains untracked in git.

---

Source lives under `app/` (gitignored, D-003) — this file is the tracked-repo record
of that change, not a substitute for reading the actual code.
