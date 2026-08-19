# Issue #41: Add boss-fight music intensity swap on twist-mechanic telegraph

`UMusicSubsystem` (issue #25) only crossfaded between `Calm`/`Combat`, driven by
polling `AEnemyBase` threat state. Boss fights against `ADualZoneBoss`/
`ASleepShieldBoss` — neither of which spawns adds — never triggered even the
baseline `Combat` track, and their twist-mechanic telegraphs (Enrage, shield-drop)
had no audio cue at all. This adds a third `EMusicState::BossIntensity` state, a
`BossIntensityTrack` config track resolved through the existing crossfade path, and
a `ABossBase::IsTwistTelegraphed()` virtual predicate (default `IsEnraged()`,
overridden by `ASleepShieldBoss` to `!HasShield()` while engaged) so each boss
subclass can define its own telegraph signal without `UMusicSubsystem` needing
subclass-specific knowledge.

## Acceptance Criteria

- [x] **`EMusicState::BossIntensity` added as a third state ("same mood/genre,
  higher energy, not a different song").** Added last (ordinal 2), `Calm`/`Combat`
  ordinals unchanged — `MusicSubsystem.h`.
- [x] **Switches from the standard combat track to the heightened boss variant the
  moment a boss's twist mechanic telegraphs.** `IsAnyBossEngaged()` establishes the
  `Combat` baseline for any Armed/Vulnerable boss even with zero regular enemies;
  `IsAnyBossTwistTelegraphed()` promotes to `BossIntensity` — `MusicSubsystem.cpp`'s
  `RefreshMusicState()`.
- [x] **Reverts once the twist-mechanic window ends, or the fight ends.** Shield
  re-raising drops back to `Combat` (still engaged); reaching `Banked` drops fully
  to `Calm` — proven for both bosses (`ASleepShieldBoss`'s naturally-clearing shield
  flag and `ADualZoneBoss`'s `IsEnraged()`, which never clears on its own but still
  reverts correctly off boss state, not the flag).
- [x] **Automation test verifying the swap syncs with the telegraph event.**
  `KrowdKontrolMusicSubsystemTest.cpp` scenarios (m)-(r), appended to the existing
  `KrowdKontrol.Unit.MusicSubsystem` test, covering both `ASleepShieldBoss`
  (shield-drop) and `ADualZoneBoss` (Enrage) end-to-end.

## Validation

`python harness/ci.py` → `GATE_OK mode=full` (`UNIT_PASSED tests=64`,
`UE_AUTOMATION_RESULT passed=1 total=1`), passed on the first run. Confirmed the
real `app/` symlink carries the same edits as this mirror (no drift, no
concurrent-task leakage). `ARootSurgeBoss` — unrelated in-flight work sharing the
`app/` symlink — is untouched by this change. Full detail in this run's
`validation.md`.
