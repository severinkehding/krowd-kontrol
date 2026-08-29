# Issue #358: Route SN-1PR sniper shot through ApplyContactDamage for real player damage

`ASniperEnemy::AdvanceAttackTelegraph()` already broadcast `OnSniperShotFired` the
instant its attack telegraph elapsed, but nothing consumed that moment to actually
hurt the player - the sniper's attack tell and audio cue were a pure light-and-sound
show with no real consequence. This change wires the existing shot-fired moment
straight into `UPlayerEnergyComponent::ApplyContactDamage()`, the codebase's sole
legal energy mutator, using the same `FindPlayerEnergyComponent()` primitive
`ABomberEnemy::TriggerExplosion()` and `ARootSurgeBoss::AdvanceAttackTelegraph()`
already call.

A new `EditDefaultsOnly` tunable, `ASniperEnemy::ShotDamageAmount`, controls the
per-shot cost. It is set to **8.0f** for this first pass - deliberately at or below
`UPlayerEnergyComponent::MaxDamagePerHit` (10.0f) so a landed shot's observed energy
loss is exactly the named constant, not the clamp ceiling (the opposite choice from
`ABomberEnemy::ExplosionDamageAmount` / `ARootSurgeBoss::AttackDamageAmount`, which
both deliberately exceed the clamp and rely on it for their non-lethality
guarantee). This value is a first-pass placeholder, flagged here explicitly for
operator playtest/balance review - it is not a final balance decision.

No new delegate, drain mechanism, or "on-hit" event was added - `OnSniperShotFired`
already existed and remains the sole extension point; the damage call sits directly
after its existing broadcast, unconditionally, with no range re-check (SN-1PR's
`GetAttackRangeUnits()` is deliberately near its detection range specifically so it
never needs one - see `SniperEnemy.cpp`'s existing comment on that value).
`FindPlayerEnergyComponent()` already returns `nullptr` (tolerating a null
`GetWorld()`, and logging a warning) when no `UPlayerEnergyComponent`-carrying pawn
exists, which is the existing, already-tested mechanism satisfying the "no phantom
hits when no valid target exists" requirement - zero new code needed for that case.

Issue #222's HUD hit-frame reaction required no changes: `UEnergyMeterWidget` already
binds generically to `UPlayerEnergyComponent::OnEnergyChanged`, so it fires
automatically off this new `ApplyContactDamage` call with nothing sniper-specific to
wire. Issue #336's attack-window derivation (`ASniperEnemy::GetAttackTelegraphSeconds()`)
is also untouched - this change only extends what happens once the telegraph already
reaches zero, not when or how it does so.

## Verifiable evidence

All snippets below were re-read directly from the live `app/` source at the time
this file was written.

**The extended shot-fired broadcast site, `Source/KrowdKontrol/SniperEnemy.cpp:153-176`:**

```cpp
void ASniperEnemy::AdvanceAttackTelegraph(float DeltaSeconds)
{
	if (bShotFiredForCurrentAttack || !IsAttackBehaviorActive())
	{
		return;
	}
	RemainingTelegraphSeconds = FMath::Max(0.0f, RemainingTelegraphSeconds - DeltaSeconds * GetControlledSpeedMultiplier());
	if (RemainingTelegraphSeconds <= 0.0f)
	{
		// Guards against re-firing OnSniperShotFired every subsequent tick once the
		// telegraph reaches zero - the same "fire exactly once" shape
		// ABossBase::TransitionToBanked/FOnBossBanked already established for this
		// module's delegates.
		bShotFiredForCurrentAttack = true;
		OnSniperShotFired.Broadcast();

		// Issue #358: the shot's actual consequence. FindPlayerEnergyComponent()
		// tolerates a null GetWorld() (true for NewObject<>()-constructed test
		// instances - see BomberEnemy.cpp's identical call site) by returning nullptr,
		// so a shot resolving with no valid/present player target applies no damage -
		// no phantom hits, no crash.
		if (UPlayerEnergyComponent* Energy = FindPlayerEnergyComponent())
		{
			Energy->ApplyContactDamage(ShotDamageAmount, this);
		}
	}
}
```

**The new tunable, `Source/KrowdKontrol/SniperEnemy.h:92-98`:**

```cpp
	// SN-1PR's first-pass per-hit damage value (issue #358) - deliberately at or below
	// UPlayerEnergyComponent::MaxDamagePerHit (10.0f) so a landed shot always costs
	// exactly this amount, not the clamp ceiling (contrast ABomberEnemy::
	// ExplosionDamageAmount / ARootSurgeBoss::AttackDamageAmount, which both
	// deliberately exceed the clamp instead). Subject to operator playtest tuning.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0.0"))
	float ShotDamageAmount = 8.0f;
```

**New regression coverage, `Private/Tests/KrowdKontrolSniperEnemyTest.cpp` cases (t)
and (u)** (added after the file's existing case (s), before `return true;`):

- (t) drives a sniper to Attack in a real `UWorld` with a spawned player `APawn`
  carrying a manually-registered `UPlayerEnergyComponent`, fires the telegraph via
  `AdvanceAttackTelegraph(AttackTelegraphSeconds)`, and asserts
  `Energy->GetCurrentEnergy() == EnergyBeforeShot - Sniper->ShotDamageAmount`
  (exact-constant, not clamp-ceiling, damage).
- (u) drives a sniper to Attack in a real `UWorld` with no player pawn present at
  all, fires the telegraph, and asserts `OnSniperShotFired` still fired exactly once
  (the delegate's own firing behavior is unchanged) with no crash - proving the "no
  valid/present target" case is a safe no-op.

All pre-existing cases (a)-(s) construct their `ASniperEnemy` instances via
`NewObject<>()` with no `UWorld`, so `FindPlayerEnergyComponent()`'s null-`GetWorld()`
tolerance makes the new damage call a no-op for every one of them - no regression,
no new case needed to prove it.

## Design decisions

- **8.0f, not a value exceeding `MaxDamagePerHit`.** Unlike Bomber/RootSurge, this
  issue's own regression-test acceptance criterion ("assert player energy decreases
  by exactly the tunable constant") only holds if the constant is at or below the
  clamp ceiling - otherwise the observed decrease is the clamp, not the constant.
- **No range-gating on the damage call**, matching the issue's explicit "do not
  change when or how the tell/delegate fires" scope boundary - SN-1PR's own
  `GetAttackRangeUnits()` is deliberately kept close to its detection range so it
  doesn't need one (unlike `ARootSurgeBoss`, which does re-check range here).
- **No new delegate/drain mechanism.** `OnSniperShotFired` remains the only signal;
  `ApplyContactDamage` remains the only energy mutator touched.

## Acceptance criteria

- [x] `AdvanceAttackTelegraph()` calls `ApplyContactDamage` on the player's
      `UPlayerEnergyComponent` at the moment `OnSniperShotFired` broadcasts
- [x] Damage amount is a single named, `EditDefaultsOnly` constant
      (`ShotDamageAmount = 8.0f`), with an inline comment giving its current value
      and noting it is subject to operator playtest tuning
- [x] `ApplyContactDamage` remains the only energy mutator touched
- [x] A shot resolving with no valid/present player target applies no damage and
      does not crash (case (u))
- [x] `UEnergyMeterWidget`'s existing hit-frame reaction fires correctly off this
      call with zero changes to `EnergyMeterWidget.cpp`/`.h`
- [x] New regression test asserts energy decreases by exactly `ShotDamageAmount` on
      a landed shot (case (t)), and does not decrease when there is no valid target
      (case (u))
- [x] This file discloses the chosen damage-per-hit value (8.0f) for balance review
- [x] Every edit to `app/Source/KrowdKontrol/...` has a matching, identical edit
      under `app-source-tracked/Source/KrowdKontrol/...`

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.

## Operator resolution of the pass-2 escalation (2026-08-29)

**The E2E holdout was right — and the cause was concurrency, not this PR's
code.** The concurrent #387/#388 sniper work (telegraph + range-break chase)
rewrote `SniperEnemy.h/.cpp` in the shared `app/` after this branch diverged,
erasing this PR's `ShotDamageAmount`/`ApplyContactDamage` wiring from the live
build — which is exactly what the holdout's isolated test then observed as a
confirmed full telegraph cycle with zero energy change. Resolution: the damage
wiring and all three test cases ((t)/(u)/(u2)) were re-ported operator-side
onto the current post-#388 code (fire block now hides the telegraph indicator,
broadcasts, then applies damage), and the branch merged with main so the
mirror matches the live union. Verified: clean build, sniper suite green, full
unit suite 127/127. The Bomber-adjacent-spawn E2E confound in L_Level02 is
noted for future holdout runs but needs no code change.
