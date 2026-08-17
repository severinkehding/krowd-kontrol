# Issue #33: Attack-telegraph audio tell for B0-0MR

Adds a one-shot audio cue at `ABomberEnemy::OnAttackEntry()`, the same extension point
the existing visual tell (`AttackTellLightComponent`) already uses. Mirrors PR #143's
identical feature for SN-1PR (`ASniperEnemy`, issue #36) near-verbatim: a
`TSoftObjectPtr<USoundBase>` property (`AttackTellSound`), defaulted in the
constructor via `ConstructorHelpers::FObjectFinder` to a built-in engine sound so the
"a distinct sound effect plays" AC holds out of the box, resolved via
`LoadSynchronous()` and played through `UGameplayStatics::SpawnSoundAtLocation()`.
B0-0MR's default points at `/Engine/EngineSounds/WhiteNoise` rather than SN-1PR's
`1kSineTonePing`, so the two enemy types' tells are audibly distinct (a broadband
noise burst vs. a clean sine tone).

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `BomberEnemy.h` | UPDATE | `AttackTellSound` (`TSoftObjectPtr<USoundBase>`), private `AttackTellAudioComponent` (`TObjectPtr<UAudioComponent>`, test-visible via the existing `FKrowdKontrolBomberEnemyTest` friend grant), private `bHasWarnedMissingAttackTellSound` one-shot guard, forward declarations for `USoundBase`/`UAudioComponent` |
| `BomberEnemy.cpp` | UPDATE | Constructor default via `ConstructorHelpers::FObjectFinder` pointed at `/Engine/EngineSounds/WhiteNoise.WhiteNoise`; `OnAttackEntry()` resolves `AttackTellSound` and spawns it via `SpawnSoundAtLocation`, falling back to a one-shot `UE_LOG` warning if unset; new includes for `Sound/SoundBase.h`, `Kismet/GameplayStatics.h`, `Components/AudioComponent.h` |
| `Private/Tests/KrowdKontrolBomberEnemyTest.cpp` | UPDATE | Cases (p)/(q)/(r): a configured `AttackTellSound` spawns an audio cue with the matching asset; the constructor's `WhiteNoise` default also spawns a cue out of the box (not silent); an explicitly-cleared `AttackTellSound` spawns nothing and does not crash. New includes for `Sound/SoundWave.h`, `Components/AudioComponent.h` |

Both `app/` (the real, untracked Unreal project reached via the `app/` symlink) and
this `app-source-tracked/` mirror were updated identically.

## Acceptance criteria

- [x] "A distinct sound effect plays at the start of B0-0MR's attack telegraph
      window (same window driving the existing visual tell)" - `OnAttackEntry()`
      calls `SpawnSoundAtLocation` when `AttackTellSound` resolves, and the
      constructor now defaults it to a real, playing `WhiteNoise` asset (case (q)),
      not left unset.
- [x] "Clearly distinguishable from the other 3 enemy types' attack tells and from
      existing ability-cast/UI sounds" - `WhiteNoise` (broadband noise burst) is
      audibly distinct from SN-1PR's `1kSineTonePing` (clean sine tone), the only
      other enemy with an audio tell configured today.
- [x] "Plays once per telegraph - not looping, not replayed if the attack is
      interrupted or the enemy is controlled mid-telegraph" - satisfied by
      `OnAttackEntry()`'s existing single-fire guarantee plus
      `SpawnSoundAtLocation`'s non-looping default. No new interrupt-handling logic
      was added or needed.
- [x] "A `KrowdKontrol.Unit.*` Automation Framework test confirms the audio cue
      fires when B0-0MR enters its attack-telegraph state" - cases (p)/(q)/(r).

## Not built

- **Sourcing/authoring a bespoke sound asset for B0-0MR.** `AttackTellSound`
  defaults to the built-in engine `WhiteNoise` asset - a real, playing placeholder,
  not a hand-authored sound. Picking/importing a genuinely designed sound is a
  content/audio decision outside this code change's scope (same call issue #36
  made for SN-1PR's `1kSineTonePing`).
- **Audio tells for RU-NNR / TR-UPR.** Neither has an attack tell (visual or audio)
  implemented yet. Out of scope - this issue is B0-0MR only.
- **Explicitly stopping/fading the audio cue if the telegraph is interrupted
  mid-window.** The AC only requires "not replayed" (not "stops immediately on
  interrupt"). `SpawnSoundAtLocation`'s non-looping, `bAutoDestroy=true` default
  plus `OnAttackEntry()`'s existing single-fire guarantee already satisfy that; no
  new interrupt-handling logic is needed, mirroring issue #36's identical scope
  decision for SN-1PR.

## Validation evidence

`harness/ci.py --quick` (light inline check, run by the `implement` node):
GATE_OK mode=quick. Full-suite validation (`harness/run_ue_automation.sh
KrowdKontrol.Unit.`, expecting a real `UE_BUILD_OK` compile plus
`UE_AUTOMATION_RESULT` covering `KrowdKontrol.Unit.BomberEnemy`) is deferred to the
`dark-factory-validate` node per this factory's normal workflow split.

## Deviations from plan

None - implementation matches the investigation/plan exactly.
