# Issue #30: Attack-telegraph audio tell for TR-UPR

Adds a one-shot audio cue at `ATrooperEnemy::OnAttackEntry()`, the same extension point
the existing visual tell (`AttackTellLightComponent`) already uses. Mirrors PR #143's
(SN-1PR, issue #36) and PR #144's (B0-0MR, issue #33) near-identical feature, applied to
the third and final enemy type that needed it: a `TSoftObjectPtr<USoundBase>` property
(`AttackTellSound`), defaulted in the constructor via `ConstructorHelpers::FObjectFinder`
to a built-in engine sound so the "a distinct sound effect plays" AC holds out of the
box, resolved via `LoadSynchronous()` and played through
`UGameplayStatics::SpawnSoundAtLocation()`. Because `/Engine/EngineSounds/` only has two
loadable `USoundBase` assets and both are already claimed (Sniper's `1kSineTonePing`,
Bomber's `WhiteNoise`), TR-UPR's default points at a different built-in asset,
`/Engine/EditorSounds/Notifications/CompileSuccess`, so all three enemies' tells stay
audibly distinct. The sound fires exactly once per Attack entry - TR-UPR's rapid-refire
trait (`AdvanceAttackTelegraph` re-arming after every ray) only re-broadcasts
`OnTrooperRayFired`, it never re-calls `OnAttackEntry()`, so no special repeat-suppression
logic was needed beyond what Sniper/Bomber already have.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `TrooperEnemy.h` | UPDATE | `AttackTellSound` (`TSoftObjectPtr<USoundBase>`), private `AttackTellAudioComponent` (`TObjectPtr<UAudioComponent>`, test-visible via the existing `FKrowdKontrolTrooperEnemyTest` friend grant), private `bHasWarnedMissingAttackTellSound` one-shot guard, forward declarations for `USoundBase`/`UAudioComponent` |
| `TrooperEnemy.cpp` | UPDATE | Constructor default via `ConstructorHelpers::FObjectFinder` pointed at `/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess`; `OnAttackEntry()` resolves `AttackTellSound` and spawns it via `SpawnSoundAtLocation`, falling back to a one-shot `UE_LOG` warning if unset; new includes for `Sound/SoundBase.h`, `Kismet/GameplayStatics.h`, `Components/AudioComponent.h` |
| `Private/Tests/KrowdKontrolTrooperEnemyTest.cpp` | UPDATE | Cases (n)/(o)/(p)/(q): a configured `AttackTellSound` spawns an audio cue with the matching asset; the constructor's default also spawns a cue out of the box and differs from both siblings' defaults; an explicitly-cleared `AttackTellSound` spawns nothing and does not crash; a `ReceiveControl`-triggered mid-telegraph interrupt does not re-spawn the audio cue or fire another ray. New includes for `Sound/SoundWave.h`, `Components/AudioComponent.h` |

Both `app/` (the real, untracked Unreal project reached via the `app/` symlink) and
this `app-source-tracked/` mirror were updated identically.

## Acceptance criteria

- [x] "A distinct sound effect plays at the start of TR-UPR's attack telegraph
      window (same window driving the existing visual tell)" - `OnAttackEntry()`
      calls `SpawnSoundAtLocation` when `AttackTellSound` resolves, and the
      constructor now defaults it to a real, playing `CompileSuccess` asset (case
      (o)), not left unset.
- [x] "Clearly distinguishable from the other 2 enemy types' attack tells and from
      existing ability-cast/UI sounds" - `CompileSuccess` differs from both SN-1PR's
      `1kSineTonePing` and B0-0MR's `WhiteNoise` (case (o)'s two `TestNotEqual`
      assertions); no ability-cast/UI sound system exists yet in this codebase to
      compare against (same gap #33/#36's changelogs already document).
- [x] "Plays once per telegraph - not looping, not replayed if the attack is
      interrupted or the enemy is controlled mid-telegraph" - satisfied by
      `OnAttackEntry()`'s existing single-fire guarantee plus
      `SpawnSoundAtLocation`'s non-looping default, and verified directly by case
      (q), which also confirms TR-UPR's rapid re-arm loop doesn't trigger a second
      spawn. No new interrupt-handling logic was added or needed.
- [x] "A `KrowdKontrol.Unit.*` Automation Framework test confirms the audio cue
      fires when TR-UPR enters its attack-telegraph state" - cases (n)/(o).

## Not built

- **Sourcing/authoring a bespoke sound asset for TR-UPR.** `AttackTellSound`
  defaults to the built-in engine `CompileSuccess` asset - a real, playing
  placeholder, not a hand-authored sound. Same call issues #36/#33 made for their
  own defaults.
- **Audio tell for RU-NNR.** `ARunnerEnemy` has a visual tell but no
  `AttackTellSound` yet - out of scope, this issue is TR-UPR only.
- **Explicitly stopping/fading the audio cue if the telegraph is interrupted
  mid-window.** The AC only requires "not replayed" (not "stops immediately on
  interrupt"). `SpawnSoundAtLocation`'s non-looping, `bAutoDestroy=true` default
  plus `OnAttackEntry()`'s existing single-fire guarantee already satisfy that; no
  new interrupt-handling logic is needed, mirroring issues #36/#33's identical
  scope decision.
- **Making the audio tell repeat per rapid-fire ray.** The audio tell fires once,
  at Attack entry, matching the AC's "plays once per telegraph" wording and the
  visual tell's own one-shot-then-stays-on behavior; the sound-play call was
  deliberately kept out of `AdvanceAttackTelegraph()`'s repeat loop.
- **`EnemyTypeIndicatorComponent` on `ATrooperEnemy`.** A pre-existing gap
  unrelated to this issue, not touched here.

## Validation evidence

`harness/ci.py --quick` (light inline check, run by the `implement` node):
GATE_OK mode=quick. Full-suite validation (`harness/run_ue_automation.sh
KrowdKontrol.Unit.`, expecting a real `UE_BUILD_OK` compile plus
`UE_AUTOMATION_RESULT` covering `KrowdKontrol.Unit.TrooperEnemy`) is deferred to the
`dark-factory-validate` node per this factory's normal workflow split.

## Deviations from plan

None - implementation matches the investigation/plan exactly.
