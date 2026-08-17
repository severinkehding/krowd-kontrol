# Issue #28: Attack-telegraph audio tell for RU-NNR

Adds a one-shot audio cue at `ARunnerEnemy::OnAttackEntry()`, the same extension point
the existing visual tell (`AttackTellLightComponent`) already uses. Mirrors PR #143's
(SN-1PR, issue #36), PR #144's (B0-0MR, issue #33), and PR #145's (TR-UPR, issue #30)
near-identical feature, applied to the fourth and final core enemy type that needed it
(MISSION.md Hard Invariant 5: exactly 4 core types): a `TSoftObjectPtr<USoundBase>`
property (`AttackTellSound`), defaulted in the constructor via
`ConstructorHelpers::FObjectFinder` to a built-in engine sound so the "a distinct sound
effect plays" AC holds out of the box, resolved via `LoadSynchronous()` and played
through `UGameplayStatics::SpawnSoundAtLocation()`. Because `/Engine/EngineSounds/`'s
two loadable `USoundBase` assets are already claimed (Sniper's `1kSineTonePing`,
Bomber's `WhiteNoise`) and `/Engine/EditorSounds/Notifications/CompileSuccess` is
already claimed by Trooper, RU-NNR's default points at a fourth built-in asset in the
same notification-chime family, `/Engine/EditorSounds/Notifications/CompileFailed`, so
all four enemies' tells stay audibly distinct. The sound fires exactly once per Attack
entry - RU-NNR follows Sniper/Bomber's fire-once cadence (not Trooper's rapid re-arm),
and `AEnemyBase`'s strictly linear state machine (no edge back into `Attack`) means
`OnAttackEntry()` structurally cannot fire twice for one attack, so no new
interrupt-handling logic was needed.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `RunnerEnemy.h` | UPDATE | `AttackTellSound` (`TSoftObjectPtr<USoundBase>`), private `AttackTellAudioComponent` (`TObjectPtr<UAudioComponent>`, test-visible via the existing `FKrowdKontrolRunnerEnemyTest` friend grant), private `bHasWarnedMissingAttackTellSound` one-shot guard, forward declarations for `USoundBase`/`UAudioComponent` |
| `RunnerEnemy.cpp` | UPDATE | Constructor default via `ConstructorHelpers::FObjectFinder` pointed at `/Engine/EditorSounds/Notifications/CompileFailed.CompileFailed`; `OnAttackEntry()` resolves `AttackTellSound` and spawns it via `SpawnSoundAtLocation`, falling back to a one-shot `UE_LOG` warning if unset; new includes for `Sound/SoundBase.h`, `Kismet/GameplayStatics.h`, `Components/AudioComponent.h` |
| `Private/Tests/KrowdKontrolRunnerEnemyTest.cpp` | UPDATE | Cases (n)/(o)/(p)/(q): a configured `AttackTellSound` spawns an audio cue with the matching asset; the constructor's default also spawns a cue out of the box and differs from all 3 siblings' *live* defaults (`NewObject<>()` comparisons, not hardcoded strings, per PR #145's review-fix precedent); an explicitly-cleared `AttackTellSound` spawns nothing and does not crash; a `ReceiveControl`-triggered mid-telegraph interrupt does not re-spawn the audio cue. New includes for `Sound/SoundWave.h`, `Components/AudioComponent.h`, `SniperEnemy.h`, `BomberEnemy.h`, `TrooperEnemy.h` |

Both `app/` (the real, untracked Unreal project reached via the `app/` symlink) and
this `app-source-tracked/` mirror were updated identically.

## Acceptance criteria

- [x] "A distinct sound effect plays at the start of RU-NNR's attack telegraph
      window (same window driving the existing visual tell)" - `OnAttackEntry()`
      calls `SpawnSoundAtLocation` when `AttackTellSound` resolves, and the
      constructor now defaults it to a real, playing `CompileFailed` asset (case
      (o)), not left unset.
- [x] "Clearly distinguishable from the other 3 enemy types' attack tells and from
      existing ability-cast/UI sounds" - `CompileFailed` differs from SN-1PR's
      `1kSineTonePing`, B0-0MR's `WhiteNoise`, and TR-UPR's `CompileSuccess` (case
      (o)'s three `TestNotEqual` comparisons against live siblings); no
      ability-cast/UI sound system exists yet in this codebase to compare against
      (same gap #36/#33/#30's changelogs already document).
- [x] "Plays once per telegraph - not looping, not replayed if the attack is
      interrupted or the enemy is controlled mid-telegraph" - satisfied by
      `OnAttackEntry()`'s structural single-fire guarantee (`AEnemyBase`'s linear
      state machine has no edge back into `Attack`) plus `SpawnSoundAtLocation`'s
      non-looping default, and verified directly by case (q). No new
      interrupt-handling logic was added or needed.
- [x] "A `KrowdKontrol.Unit.*` Automation Framework test confirms the audio cue
      fires when RU-NNR enters its attack-telegraph state" - cases (n)/(o).

## Not built

- **Sourcing/authoring a bespoke sound asset for RU-NNR.** `AttackTellSound`
  defaults to the built-in engine `CompileFailed` asset - a real, playing
  placeholder, not a hand-authored sound. Same call issues #36/#33/#30 made for
  their own defaults.
- **Explicitly stopping/fading the audio cue if the telegraph is interrupted
  mid-window.** The AC only requires "not replayed" (not "stops immediately on
  interrupt"). `SpawnSoundAtLocation`'s non-looping, `bAutoDestroy=true` default
  plus `OnAttackEntry()`'s existing single-fire guarantee already satisfy that; no
  new interrupt-handling logic is needed, mirroring issues #36/#33/#30's identical
  scope decision.
- **A comparison against a real ability-cast/UI sound system.** No such system
  exists in this codebase yet (documented gap in #33/#30's changelogs) - the AC is
  satisfied by comparing against every hardcoded sound asset that does exist (the
  3 sibling enemies' tells), same as precedent.
- **`Content/Audio/` folder or any custom `.uasset` authoring.** No such folder
  exists anywhere in this codebase; all 4 core enemies' tells are built-in Engine
  asset references. Introducing a new asset-authoring pipeline is out of scope for
  this issue.
- **Making the audio tell repeat per-tick or loop.** It fires exactly once, at
  Attack entry, matching the visual tell's one-shot-then-stays-on shape and the
  AC's "not looping" wording.

## Validation evidence

`harness/ci.py --quick` (light inline check, run by the `implement` node):
GATE_OK mode=quick. Full-suite validation (`harness/run_ue_automation.sh
KrowdKontrol.Unit.`, expecting a real `UE_BUILD_OK` compile plus
`UE_AUTOMATION_RESULT` covering `KrowdKontrol.Unit.RunnerEnemy`) is deferred to the
`dark-factory-validate` node per this factory's normal workflow split.

## Deviations from plan

None - implementation matches the investigation/plan exactly.
