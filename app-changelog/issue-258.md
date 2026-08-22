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

`app/Config/DefaultInput.ini` has no `app-source-tracked/` mirror (only `.h`/`.cpp`/
`.Build.cs` source files get mirrored per D-009), so this diff-free excerpt of the
`[/Script/Engine.InputSettings]` `ActionMappings` block is the only way to confirm
from the tracked repo that the legacy `One`..`Five` bindings were left untouched and
the five new OG-GDD bindings were added alongside them.

Before (legacy 1-5 bindings only):

```ini
[/Script/Engine.InputSettings]
...
+ActionMappings=(ActionName="CastStun",Key=One)
+ActionMappings=(ActionName="CastSleep",Key=Two)
+ActionMappings=(ActionName="CastRoot",Key=Three)
+ActionMappings=(ActionName="CastFear",Key=Four)
+ActionMappings=(ActionName="CastSnare",Key=Five)
```

After (legacy entries unchanged, five OG-GDD entries appended):

```ini
[/Script/Engine.InputSettings]
...
+ActionMappings=(ActionName="CastStun",Key=One)
+ActionMappings=(ActionName="CastSleep",Key=Two)
+ActionMappings=(ActionName="CastRoot",Key=Three)
+ActionMappings=(ActionName="CastFear",Key=Four)
+ActionMappings=(ActionName="CastSnare",Key=Five)
+ActionMappings=(ActionName="CastStun",Key=LeftMouseButton)
+ActionMappings=(ActionName="CastSleep",Key=RightMouseButton)
+ActionMappings=(ActionName="CastRoot",Key=Q)
+ActionMappings=(ActionName="CastFear",Key=MiddleMouseButton)
+ActionMappings=(ActionName="CastSnare",Key=E)
```

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

**Full-suite validation now confirmed** — the implement session's WSL worktree
could not reach a live Unreal Editor/MCP session (recurring worktree↔host
network-path gap), but that gap is specific to the *live MCP* connection, not to
headless `UnrealEditor-Cmd.exe` invocation. The `dark-factory-validate` session ran
`python harness/ci.py` in full mode, which rebuilds `KrowdKontrolEditor` and runs
the real Automation Framework headlessly via `harness/run_ue_automation.sh
KrowdKontrol.Unit.`: `UE_BUILD_OK` followed by `UNIT_PASSED tests=88`, with no
failures — this includes `KrowdKontrol.Unit.FlatCamera3DPipelineSmoke` and its new
`UInputSettings::GetActionMappings()` assertions for both the original 1-5 keys and
the new OG-GDD keys. Gate: `GATE_OK mode=full`.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — this only adds alternate key bindings to already-existing
abilities via already-existing routing.

## E2E test environment (pass-1 fix)

Pass-1's E2E holdout could not verify the 5 new key bindings route through the
existing `UAbilityCastComponent::TryCastAbility` entry point (rather than some
forked path) because the only Editor session it could reach had loaded a blank
`Untitled` level with a stock `DefaultPawn` — there was no `FlatCamera3DPrototypePawn`
or enemy target present to press LMB/RMB/Q/E/MMB against and observe a cast.

Root cause: `[/Script/EngineSettings.GameMapsSettings]` in
`app/Config/DefaultEngine.ini` had no `EditorStartupMap`, so a freshly-launched
Editor (as `scripts/ue_editor_launch_and_wait.sh` does before every E2E holdout run)
opened whatever blank level the Editor defaults to rather than a populated one.
`app/Content/Maps/L_Level01.umap` already exists and contains a
`FlatCamera3DPrototypePawn` plus 6 enemy actors (3 `BomberEnemy`, 1 `RunnerEnemy`, 2
`TrooperEnemy`) — the smallest of this project's populated gameplay levels with both
a player pawn and enemy targets — so pointing `EditorStartupMap` at it, rather than
adding a new level, is the minimal fix.

Like `DefaultInput.ini`, `DefaultEngine.ini` has no `app-source-tracked/` mirror
(D-009 only mirrors `.h`/`.cpp`/`.Build.cs`), so this before/after excerpt is the only
diff-visible record of the change:

Before:

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Engine/Maps/Templates/OpenWorld
GlobalDefaultGameMode=/Script/KrowdKontrol.KrowdKontrolGameMode
```

After:

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Engine/Maps/Templates/OpenWorld
GlobalDefaultGameMode=/Script/KrowdKontrol.KrowdKontrolGameMode
EditorStartupMap=/Game/Maps/L_Level01
```

This only changes which level the Editor opens on a fresh launch — it does not touch
`GameDefaultMap` (packaged/PIE-without-override runtime start) or any gameplay code,
so it carries no MISSION.md Hard Invariant risk.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
