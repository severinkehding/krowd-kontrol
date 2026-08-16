# Issue #25: 2-state calm/combat adaptive music system

Adds `UMusicSubsystem` (a `UTickableWorldSubsystem`) that plays a calm placeholder
track by default and crossfades to a combat placeholder track the instant any spawned
`AEnemyBase` reports itself "Hot" via the existing (previously unimplemented)
`IThreatState` interface, reverting to calm the instant no enemy is Hot.
`AEnemyBase` gets its first real `IThreatState` implementer (`GetThreatState()`,
mapping Alert/Attack/Controlled -> Hot, Idle/Banked -> Idle), closing a gap flagged
since issue #81. Two short, procedurally generated placeholder `.wav` loops (80s
electronic, driving bass character - sparse 90 BPM calm bassline vs. faster 140 BPM
combat bassline) stand in for real music per MISSION.md's placeholder-first rule.

## Files changed

| File | Action | Contains |
|------|--------|----------|
| `EnemyBase.h`/`.cpp` | UPDATE | `AEnemyBase` implements `IThreatState`; `GetThreatState()` mapping; new friend grant for the music-subsystem test |
| `MusicSubsystem.h`/`.cpp` | CREATE | `EMusicState`, `FOnMusicStateChanged`, `UMusicSubsystem`: tick-driven aggregation over `TActorIterator<AEnemyBase>`, `FadeOut`/`FadeIn` crossfade |
| `Private/Tests/MusicStateTestListener.h`/`.cpp` | CREATE | Test-only dynamic-delegate listener for `OnMusicStateChanged` |
| `Private/Tests/KrowdKontrolMusicSubsystemTest.cpp` | CREATE | `KrowdKontrol.Unit.MusicSubsystem`, cases (a)-(i): default state, single-enemy full Idle->Alert->Attack->Controlled->Banked cycle, idempotent no-op refresh, two-enemy aggregation |

Not mirrored here (per Hard Invariant #8's carve-out - `app-source-tracked/` never
holds binary/content assets): `app/Content/_Placeholder/Music/PlaceholderCalmTrack.wav`
and `PlaceholderCombatTrack.wav`, two procedurally generated placeholder tracks that
exist on disk under `app/` but are not yet imported into the Editor as `USoundWave`
assets (see Known Follow-Up below).

## Acceptance criteria

- [x] A calm-state and a combat-state placeholder music track exist in the project:
      `app/Content/_Placeholder/Music/PlaceholderCalmTrack.wav` /
      `PlaceholderCombatTrack.wav`, procedurally generated (no licensing risk).
- [x] `UMusicSubsystem` plays the calm track by default (cleared rooms, hub/menu):
      `CurrentState = EMusicState::Calm` (`MusicSubsystem.h`), trivially true with
      zero/no-Hot enemies present.
- [x] The moment any `AEnemyBase` reports `IThreatState::Hot`, playback switches to
      the combat track; reverts to calm when no enemy is Hot:
      `UMusicSubsystem::RefreshMusicState()` / `IsAnyEnemyInCombat()`
      (`MusicSubsystem.cpp`).
- [x] The switch is a `FadeOut`/`FadeIn` crossfade over `CrossfadeDurationSeconds`,
      never an abrupt `Stop()`/instant swap: `SetMusicState()` (`MusicSubsystem.cpp`).
- [x] Exactly 2 discrete states (`EMusicState::Calm`/`Combat`) - no 3rd state, no
      stems/layering: `MusicSubsystem.h`.
- [x] `KrowdKontrol.Unit.MusicSubsystem` Automation Framework test passes and
      confirms the state switches correctly on hot/idle transitions:
      `KrowdKontrolMusicSubsystemTest.cpp`, 9 cases.
- [x] Level 1-2 validation commands pass: Editor/UnrealBuildTool rebuild succeeded
      (0 errors), `harness/ci.py` full mode `GATE_OK`.
- [x] No regressions in `KrowdKontrol.Unit.EnemyBase` or `KrowdKontrol.Unit.ThreatState`:
      both still present and passing in the same gate run.
- [x] No Hard Invariant from MISSION.md violated: no kill path touched (`Banked`
      still maps to `Idle`, never destroyed), no 6th gameplay colour introduced,
      ability/enemy rosters untouched, engine/2D lock untouched.
- [ ] Editor-side WAV import (`USoundWave` assets) + `DefaultGame.ini` wiring — see
      Known Follow-Up below. `SetMusicState()` degrades gracefully with unset tracks,
      so this does not block the state-machine/test half of this issue.

## Validation evidence

```
$ python harness/ci.py
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=29
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

`unit=29` (28 pre-change baseline + 1 new `KrowdKontrol.Unit.MusicSubsystem`),
confirmed passing by name in `app/Saved/Logs/KrowdKontrol.log`. Editor module
rebuilt from scratch via `UnrealBuildTool` before this run to confirm the new/changed
files actually compile - 0 errors. No re-runs were needed; green on the first
full-mode pass.

## Known follow-up (not blocking)

The two placeholder `.wav` files exist on disk at `app/Content/_Placeholder/Music/`
but were not imported into the Unreal Editor as `USoundWave` assets this session -
the session's `unreal-mcp` client connection never came up even though the Editor
itself was confirmed live (see `implementation.md`'s Deviations section for the full
account). `DefaultGame.ini` was deliberately left unmodified rather than pointing
`CalmTrack`/`CombatTrack` at asset paths that don't exist yet. `SetMusicState()`
already degrades gracefully with unset tracks (proven by every Automation test case,
none of which configure a track), so the state machine itself is fully correct and
testable without this step. Fast follow-up: import both WAVs as `USoundWave` under
`Content/_Placeholder/Music/`, then add
`[/Script/KrowdKontrol.MusicSubsystem]` / `CalmTrack=...` / `CombatTrack=...` to
`app/Config/DefaultGame.ini`.

## Deviations from plan

None on Tasks 1-6 (the C++ system and its test) - implemented exactly as planned.
Task 7's Editor-import half is incomplete per the Known Follow-Up above; the plan's
own GOTCHA for that task explicitly anticipated and permitted this split.
