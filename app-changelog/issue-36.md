# Issue #36: Attack-telegraph audio tell for SN-1PR

Adds a one-shot audio cue at `ASniperEnemy::OnAttackEntry()`, the same extension point
the existing visual tell (`AttackTellLightComponent`) already uses. Follows the
placeholder-first, soft-object-pointer pattern `UMusicSubsystem` (issue #25)
established: a `TSoftObjectPtr<USoundBase>` property (`AttackTellSound`), resolved via
`LoadSynchronous()` and played through `UGameplayStatics::SpawnSoundAtLocation()`, left
unset as a normal non-error state until a real, per-enemy-type-distinguishable sound
asset is sourced (explicitly out of scope for this change - see "Not built" below).

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `SniperEnemy.h` | UPDATE | `AttackTellSound` (`TSoftObjectPtr<USoundBase>`), private `AttackTellAudioComponent` (`TObjectPtr<UAudioComponent>`, test-visible via the existing `FKrowdKontrolSniperEnemyTest` friend grant), private `bHasWarnedMissingAttackTellSound` one-shot guard, forward declarations for `USoundBase`/`UAudioComponent` |
| `SniperEnemy.cpp` | UPDATE | `OnAttackEntry()` resolves `AttackTellSound` and spawns it via `SpawnSoundAtLocation`, falling back to a one-shot `UE_LOG` warning if unset; new includes for `Sound/SoundBase.h`, `Kismet/GameplayStatics.h`, `Components/AudioComponent.h` |
| `Private/Tests/KrowdKontrolSniperEnemyTest.cpp` | UPDATE | Cases (n)/(o): a configured `AttackTellSound` spawns an audio cue on Attack entry; an unset one spawns nothing and does not crash. New include for `Sound/SoundWave.h` |

Both `app/` (the real, untracked Unreal project reached via the `app/` symlink) and
this `app-source-tracked/` mirror were updated identically.

## Acceptance criteria

- [x] "A distinct sound effect plays at the start of SN-1PR's attack telegraph
      window" - `OnAttackEntry()` calls `SpawnSoundAtLocation` when `AttackTellSound`
      resolves. The asset itself is deferred (see "Not built").
- [ ] "Clearly distinguishable from the other 3 enemy types' attack tells and from
      existing ability-cast/UI sounds" - structurally possible (a distinct, per-class
      `TSoftObjectPtr<USoundBase>` property), but the actual audio content decision is
      out of scope for this code change.
- [x] "Plays once per telegraph - not looping, not replayed if interrupted or
      controlled mid-telegraph" - satisfied by `OnAttackEntry()`'s existing
      single-fire guarantee (`AdvanceToAttack()` only fires the Alert->Attack edge
      once) plus `SpawnSoundAtLocation`'s non-looping default. No new
      interrupt-handling logic was added or needed.
- [x] "A `KrowdKontrol.Unit.*` Automation Framework test confirms the audio cue fires
      when SN-1PR enters its attack-telegraph state" - cases (n)/(o).

## Not built

- **Sourcing or authoring the actual sound asset.** `AttackTellSound` ships unset
  (soft-invalid), exactly like `UMusicSubsystem::CalmTrack`/`CombatTrack` shipped
  unset in issue #25 before real Pixabay-licensed tracks were imported in a follow-up
  round. Picking/importing a real, distinguishable sound is a content/audio-design
  decision outside an autonomous code change's scope.
- **Audio for RU-NNR / TR-UPR / B0-0MR.** None of the other 3 core enemy types have
  attack tells (visual or audio) implemented yet - this issue is scoped to SN-1PR
  only.
- **Explicitly stopping/fading the sound if interrupted mid-telegraph.** The AC only
  requires "not replayed", which the existing single-fire guard on `OnAttackEntry()`
  already satisfies.

## Validation evidence

`harness/ci.py --quick` (light inline check, run by the `implement` node):
`GATE_OK mode=quick`, `UNIT_PASSED tests=44`, `STATIC_SKIPPED` (no `static` command
configured yet, documented bootstrap gap). Full-suite validation (`harness/ci.py`,
expected `UE_AUTOMATION_RESULT` for `KrowdKontrol.Unit.SniperEnemy`) is deferred to
the `dark-factory-validate` node per this factory's normal workflow split.

## Deviations from plan

None - implementation matches the investigation/plan exactly.
