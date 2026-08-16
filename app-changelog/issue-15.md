# Issue #15: B0-0MR Bomber enemy

Verify-and-extend feature. Adds `ABomberEnemy`, the third concrete `AEnemyBase`
(issue #12, PR #116) subclass: short-range explosive attacker, Orange colour,
countered by Fear. Mirrors `ASniperEnemy` (issue #17, PR #118, unmerged read-only
prior art) almost verbatim, diverging only where B0-0MR differs (range/colour/
counter-ability), plus one new piece: the telegraph elapsing calls
`UPlayerEnergyComponent::ApplyContactDamage()` (issue #78), the only legal
player-damage mutator - clamped, floors at 0 - so the attack is provably non-lethal
per the issue's "crowd-drain" acceptance criterion.

## Files changed

| File | Action | What it contains |
|------|--------|-------------------|
| `BomberEnemy.h`/`.cpp` | CREATE | `ABomberEnemy`: sphere mesh, Fear-only Orange glow, short attack range, tell+telegraph, `TriggerExplosion()` player-damage integration |
| `Private/Tests/BomberExplodedTestListener.h`/`.cpp` | CREATE | Dynamic-delegate test listener, mirrors `SniperShotFiredTestListener` |
| `Private/Tests/KrowdKontrolBomberEnemyTest.cpp` | CREATE | `KrowdKontrol.Unit.BomberEnemy`, cases (a)-(o), mirrors Sniper's test plus (n)/(o) for the new damage path |
| `EnemyBase.h`, `PlayerEnergyComponent.h` | UPDATE | +`friend class FKrowdKontrolBomberEnemyTest;` each - friendship isn't inherited; both files already document this pattern |

## Acceptance criteria

- [x] Extends `AEnemyBase` (`BomberEnemy.h:24`).
- [x] Sphere silhouette distinguishable from Cube/Cylinder/Cone by shape alone (case a).
- [x] Attack tell precedes explosion, distinct colour, longer telegraph (`2.0f` vs.
      Sniper's `1.2f`) for early readability (cases e/f/k).
- [x] Short-range (`150.0f`, case j), powerful (`ExplosionDamageAmount=999.0f`,
      clamped non-lethally, case o); "slow movement" out of scope - no movement
      system exists anywhere in the codebase.
- [x] Fear-only glow intensify; every other ability, no response (cases c/d).
- [x] Orange only via `ReservedGameplayColours::GetOrange()` (`BomberEnemy.cpp:33`).
- [x] Doesn't implement Fear - only reads `EAbilitySlot Ability` (`.cpp:57-73`).
- [x] Never lethal - routes through `ApplyContactDamage`'s clamp/floor (case o).
- [x] `python harness/ci.py --quick` reports `UNIT_PASSED`, zero regressions.

## Deviations from plan

- **Scope**: "slow movement" and the "7s vs 5s lock duration" are narrative, not
  enforceable AC - Sniper's sibling issue carried identical figures unimplemented.
- **`EnemyBase.h`/`PlayerEnergyComponent.h`** were marked "NOT changed" in the plan,
  but each needed one `friend class FKrowdKontrolBomberEnemyTest;` line - friendship
  isn't inherited, and both files already document this for their existing grants.
- **`TriggerExplosion` uses `TActorIterator<APawn>`, not
  `UGameplayStatics::GetPlayerPawn()`** as planned. Found empirically: case (o)
  failed (energy unchanged), and diagnostic logging showed
  `World->GetNumControllers()==0` even after spawning+possessing a controller -
  this editor-preview `UWorld` never runs `World->BeginPlay()`, the same gap
  `KrowdKontrolRoomEnemyBudgetControllerTest.cpp` documents for component
  `BeginPlay()`. `TActorIterator` (already used in
  `KrowdKontrolFlatCamera3DPipelineSmokeTest.cpp`) needs neither. Re-verified
  passing after the fix.

## Validation evidence

Real `UnrealBuildTool` invocation (`Build.bat`/`UnrealBuildTool.exe` directly -
`run_ue_automation.sh`'s `-Cmd.exe` does not itself recompile out-of-date source,
confirmed when it first silently ran against a stale pre-Bomber DLL):
`Result: Succeeded`, 0 errors, 0 warnings.

```
$ bash harness/run_ue_automation.sh KrowdKontrol.Unit.BomberEnemy
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK

$ python harness/ci.py --quick
UNIT_PASSED tests=28
GATE_OK mode=quick
```

`28` = pre-existing `27` (issue #12/#17 baseline, Sniper's test already live in
`app/` though PR #118 is unmerged in git history) + `KrowdKontrol.Unit.BomberEnemy` -
zero regressions.
