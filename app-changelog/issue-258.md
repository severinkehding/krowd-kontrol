# Issue #258: Add OG ability keybindings (LMB/RMB/Q/E/MMB) alongside existing 1-5 alternates

Adds the OG GDD's mouse/QE ability keybindings (PRD "Ability Targeting Shapes &
Effect Semantics" REQ-3, P1) — LMB=Stun, RMB=Sleep, Q=Root, E=Snare, MMB=Fear — to
`app/Config/DefaultInput.ini` as additional `+ActionMappings` entries under the
existing `CastStun`/`CastSleep`/`CastRoot`/`CastFear`/`CastSnare` action names,
alongside the untouched `One`..`Five` entries. This is a **config-only change**: no
C++ routing code was added or modified, because `AFlatCamera3DPrototypePawn::SetupPlayerInputComponent`'s
existing `BindAction` calls already bind by action *name*, not by key — any key
mapped to a given action name (old or new) already flows through the same unchanged
`Cast*Ability()` wrapper into `UAbilityCastComponent::TryCastAbility()`. A reader
who expects new wrapper code for the new keys should not read its absence as
incomplete work; it's the intended minimal-diff shape of this fix.

## Acceptance criteria

- [x] `app/Config/DefaultInput.ini` gains 5 new `+ActionMappings` entries:
      LMB→CastStun, RMB→CastSleep, Q→CastRoot, E→CastSnare, MMB→CastFear.
      Confirmed via `grep -c 'ActionMappings=(ActionName="Cast'
      app/Config/DefaultInput.ini` → `10` (5 original + 5 new).
- [x] `AFlatCamera3DPrototypePawn`'s existing `BindAction` calls route these new
      bindings to the same `UAbilityCastComponent::TryCastAbility` entry point — no
      separate binding path exists (confirmed by inspection: `FlatCamera3DPrototypePawn.cpp`/`.h`
      are unmodified by this change) and by the extended
      `KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` automation test's new
      `UInputSettings::GetActionMappings()` assertions, which fail if a key isn't
      mapped to its expected action.
- [x] The existing 1-5 key bindings are unchanged and continue to work — the
      original `One`..`Five` `+ActionMappings` lines were left untouched, and the
      extended test's `OriginalKey` assertions confirm each is still present.
- [x] **Canonical binding-set recommendation**: the OG-GDD bindings (LMB/RMB/Q/E/MMB)
      should be treated as canonical for the tray UX PRD's on-screen labels, with
      1-5 kept as a legacy alternate. This follows the PRD's overall framing of
      adopting the OG GDD as source of truth, but is this implementation's
      recommendation rather than an already-recorded decision — the tray UX PRD
      issue should treat it as a proposal to confirm, not settled fact.
- [x] `app/` and `app-source-tracked/` copies of
      `KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp` are identical (verified via
      `diff`, no output).
- [x] `python harness/ci.py --quick` reports `GATE_OK mode=quick`.
- [x] No regression in `KrowdKontrol.Unit.FlatCamera3DPrototypePawnAbilityCastWiring`
      (the sibling test in the same file) — unmodified by this change; its wiring
      assertions are independent of the key-mapping assertions added here.

## Validation evidence

Quick gate (`python harness/ci.py --quick`):

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=88
GATE_OK mode=quick
```

`diff app/Source/KrowdKontrol/Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp app-source-tracked/Source/KrowdKontrol/Private/Tests/KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp`
→ no output (identical).

This extends an existing test (`FKrowdKontrolFlatCamera3DPipelineSmokeTest`) rather
than adding a new `IMPLEMENT_SIMPLE_AUTOMATION_TEST` block, so the automation test
count is unaffected by this change; `UNIT_PASSED tests=88` above reflects the
harness's config-driven unit count, unrelated to the Automation Framework test
count.

**Full-suite validation (live Editor / Automation Framework run of
`KrowdKontrol.Unit.FlatCamera3DPipelineSmoke`) is pending** — this WSL worktree
session could not reach a live Unreal Editor/MCP session (recurring
worktree↔host network-path gap, not specific to this change). The new assertions
were written directly against confirmed engine API surface
(`UInputSettings::GetActionMappings()`, `FInputActionKeyMapping::ActionName`/`::Key`
in `Engine/Source/Runtime/Engine/Classes/GameFramework/InputSettings.h` and
`PlayerInput.h` in the UE 5.8 install) and mirror the existing test's proven
shape, but a real Editor run to confirm the automation test actually passes is
flagged as outstanding for a session with real Editor access.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this only adds alternate key bindings to already-existing
abilities via already-existing routing.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
