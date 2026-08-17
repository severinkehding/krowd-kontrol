# Issue #14: Implement TR-UPR enemy: rapid single rays attack, Root weakness telegraph

Adds `ATrooperEnemy` (codename TR-UPR), the 4th and final core enemy type from PRD
03's roster. It extends `AEnemyBase` (issue #12) with a medium-range attack that
fires **repeatedly** while `Attack` persists — the one mechanical trait that sets it
apart from `ASniperEnemy`/`ABomberEnemy`, which each fire exactly once per attack
episode. It gets a distinct Plane-mesh silhouette (the last unclaimed
`/Engine/BasicShapes/` primitive), a Teal glow that intensifies only on
`EAbilitySlot::Root`, and its own attack-tell colour, distinct from both siblings
and all 5 reserved gameplay colours.

## Files changed (all under `app/`, gitignored per D-003 — mirrored below per D-009)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/TrooperEnemy.h` | CREATE | `ATrooperEnemy` declaration: mesh/glow/tell components, `FOnTrooperRayFired` delegate, telegraph state, `GetAttackRangeUnits`/`OnControlledEntry`/`OnAttackEntry`/`Tick` overrides |
| `app/Source/KrowdKontrol/TrooperEnemy.cpp` | CREATE | Implementation: Plane-mesh silhouette, Teal glow (Root-only intensify), magenta attack tell, 700.0f medium attack range, self-re-arming `AdvanceAttackTelegraph` (no fire-once guard) |
| `app/Source/KrowdKontrol/Private/Tests/TrooperRayFiredTestListener.h`/`.cpp` | CREATE | Dynamic-delegate test listener for `OnTrooperRayFired`, mirroring `SniperShotFiredTestListener` |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolTrooperEnemyTest.cpp` | CREATE | `KrowdKontrol.Unit.TrooperEnemy` — 14 cases (a-m, plus h2) covering silhouette, glow, tell, rapid re-fire (including a small-delta post-fire re-arm check and an oversized-delta single-fire check), range, colour non-collision, controlled-interrupt, and real-`Tick()` wiring |
| `app/Source/KrowdKontrol/EnemyBase.h` | MODIFY | Added `friend class FKrowdKontrolTrooperEnemyTest;` alongside the existing Sniper/Bomber grants, so the new test's `AdvanceToAttack` helper can call the private `TickCheckDetection` |

`EEnemyType::TR_UPR` already existed and `ARoomActor`/`UWaveSpawnerComponent`
already accept any `EEnemyType` value generically, so neither needed touching.
`ATrooperEnemy` does **not** construct a `UEnemyTypeIndicatorComponent` (issue #77's
colourblind-safe text marker) — that class lives only on the unmerged
`archon/task-fix-issue-77` branch, not on `main` or in this PR's history, and a
constructor reference to it would leave this diff unable to compile on its own. Wiring
`ATrooperEnemy` up to it is deferred to whichever PR lands second once #77 merges.

## Acceptance criteria

- [x] `ATrooperEnemy` extends `AEnemyBase`
- [x] Placeholder Plane-mesh silhouette, scaled/rotated distinctly from
      Cube/Cylinder/Cone/Sphere (test case a)
- [x] Visible attack tell precedes each ray, colour distinct from Sniper's and
      Bomber's tells and from all 5 reserved colours (test cases e/f/k)
- [x] Medium-range, rapid single-ray attacks: `GetAttackRangeUnits()` (700.0f)
      strictly between Bomber's 150.0f and Sniper's 1400.0f; telegraph fires
      repeatedly (not once) while `Attack` persists (test cases g/j)
- [x] `ReceiveControl(EAbilitySlot::Root)` — and only Root — intensifies the Teal
      glow (test cases c/d, exhaustive over all `EAbilitySlot` values)
- [x] Teal used only via `ReservedGameplayColours::GetTeal()`, never a hardcoded
      literal (Hard Invariant 3)
- [x] Does not implement Root itself — only reads the `EAbilitySlot` parameter the
      base class's existing `ReceiveControl` hook already provides
- [x] `app-source-tracked/` mirror is byte-identical to the `app/` source

## Validation

**The originally-pasted evidence below this line in earlier revisions of this file was
inaccurate** — not just because of the two compile-blocking review findings fixed
below, but because `harness/run_ue_automation.sh` boots the already-built Editor
binary and never invokes `UnrealBuildTool`; a stale `.dll` from before this PR's files
existed reports `GATE_OK` just as happily as a correct one (`KrowdKontrol.Unit.` ran
against **34** tests both before and immediately after this PR's own diff — the same
count, meaning `KrowdKontrol.Unit.TrooperEnemy` was never actually in the binary being
tested either time). Filed as a follow-up (see PR discussion) since it's a gap in the
shared harness script, out of scope for this issue to fix.

Two real review findings plus one further compile error only a real rebuild could
surface were fixed on this branch before the evidence below was captured:
1. Removed the `UEnemyTypeIndicatorComponent` member/include/construction from
   `TrooperEnemy.h`/`.cpp` — that class exists only on the unmerged issue #77 branch.
2. Added `friend class FKrowdKontrolTrooperEnemyTest;` to `EnemyBase.h` so the test's
   `AdvanceToAttack` helper can call the private `TickCheckDetection`.
3. Test case (j) called the *protected* `GetAttackRangeUnits()` on `ABomberEnemy`/
   `ASniperEnemy` instances it isn't friended to — neither review agent caught this
   because it's a real UBT compile error, invisible to a stale-binary gate run.
   Fixed by comparing against those classes' own known return values (150.0f/1400.0f)
   instead of a live cross-class call.

Evidence below is from a genuine `UnrealBuildTool` rebuild (`dotnet.exe` +
`UnrealBuildTool.dll KrowdKontrolEditor Win64 Development`, `Result: Succeeded`)
followed by a fresh `python harness/ci.py` run against that rebuilt binary — not a
stale one:

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=35
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py - NOTHING above the independence line ran.
MUTATIONS_ABSENT no harness/mutations/run.py - this gate has never been shown to fail.
GATE_OK mode=full
```

`tests=35` (up from the stale 34) confirms `KrowdKontrol.Unit.TrooperEnemy` is now
genuinely compiled in and passing — confirmed directly in
`app/Saved/Logs/KrowdKontrol.log`: `Test Completed. Result={Success} Name={TrooperEnemy}
Path={KrowdKontrol.Unit.TrooperEnemy}`. Hard invariants reviewed by inspection: no
damage/kill logic (#2), Teal used exclusively via `ReservedGameplayColours::GetTeal()`
and intensified only on Root (#3), TR-UPR fills an already-enumerated roster slot
rather than adding a 5th type (#5), and the diff lands entirely under
`app-source-tracked/` source copies (#8).

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
