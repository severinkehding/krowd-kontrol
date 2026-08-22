# Issue #221: Escalating Bomber attack-telegraph tell

`ABomberEnemy::AttackTellLightComponent` (`app/Source/KrowdKontrol/BomberEnemy.h`/`.cpp`)
previously snapped to a flat `AttackTellIntensity` on `OnAttackEntry()` and held
perfectly still for the entire ~2-second fuse, giving the player no readable signal of
how close detonation was. This change drives the tell light as a sinusoidal pulse that
speeds up and brightens as `RemainingTelegraphSeconds` counts down, and exposes a new
discrete `EBomberTelegraphStage` (`Early`/`Mid`/`Imminent`), computed each tick in the
new `UpdateTelegraphEscalation()` from the elapsed fraction of `AttackTelegraphSeconds`
against two designer-tunable thresholds (`TelegraphMidThreshold`/
`TelegraphImminentThreshold`). The new `GetAttackTelegraphStage()` accessor lets the
automation test prove the escalation is monotonic and render-independent without
sampling the oscillating pulse itself. No change to `AttackTelegraphSeconds`,
`TriggerExplosion`, `ExplosionDamageAmount`, or the `bExplodedForCurrentAttack`
single-fire guard.

## Acceptance criteria

- [x] `ABomberEnemy::AttackTellLightComponent` visibly escalates (pulsing intensity,
      increasing pulse rate) across the entire fuse rather than only signaling once at
      trigger — `UpdateTelegraphEscalation()` drives a sinusoidal pulse between a
      per-stage floor and `AttackTellIntensity` on every `AdvanceAttackTelegraph()` tick.
- [x] The escalation is distinguishable early-fuse vs late-fuse (pulse frequency
      increases `EarlyPulseFrequencyHz` (2 Hz) → `MidPulseFrequencyHz` (4 Hz) →
      `ImminentPulseFrequencyHz` (8 Hz)) — verified via new test cases (u)-(x).
- [x] No change to `AttackTelegraphSeconds`, `TriggerExplosion`'s trigger condition,
      `ExplosionDamageAmount`, or the no-kill clamp path — existing cases (g)/(h)/(g2)/(o)
      pass unmodified, confirmed by the full-mode run below.
- [x] Automation test (`KrowdKontrol.Unit.BomberEnemy`, new cases (u)-(x)) proves
      `GetAttackTelegraphStage()` changes in clearly distinguishable, monotonic
      (forward-only) steps across the fuse duration, verifiable without rendering.
- [x] Visual pulse quality itself is not asserted by the automation test — only the
      discrete `GetAttackTelegraphStage()` value is checked.
- [x] `app/` and `app-source-tracked/` copies of all changed files are byte-identical
      (`diff` clean on all three files).
- [x] `python harness/ci.py --quick` and `python harness/ci.py` both report `GATE_OK`.

## Validation evidence

Full gate (`python harness/ci.py`, mode=full):

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=82
APP_STARTED driver=cli
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Passed on the first run — no fixes required.

MISSION.md Hard Invariants reviewed against this diff: no kill-rule, colour-lock,
ability-roster, enemy-roster, engine/dimensionality, networking, or `app`-tracking
invariant is touched — `ExplosionDamageAmount`/`TriggerExplosion`'s clamp path are
untouched, and the escalation is purely a derived presentational state driven off the
existing telegraph countdown.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
