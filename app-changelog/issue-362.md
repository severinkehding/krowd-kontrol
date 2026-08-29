# Issue #362: Consolidate sniper engagement tunables

`ASniperEnemy` gathers four first-pass balance numbers introduced across three
sibling PRD issues — telegraph duration (#359), per-hit damage (#358), attack range
and chase speed (#360). Three of the four (`AttackTelegraphSeconds`,
`ShotDamageAmount`, `MovementSpeed`) were already `EditDefaultsOnly` `UPROPERTY`s on
the class; the one gap was `GetAttackRangeUnits()`, which still returned a bare
hardcoded literal (`1400.0f`) instead of a named, tunable property. This is the
"small verification-and-documentation pass" the issue's own Notes section predicted,
not a refactor: promote that one literal to a `UPROPERTY`, add the missing
first-pass/playtest comment to `AttackTelegraphSeconds` (the one property that
already existed but lacked it), add a short banner comment tying all four together
as this issue's consolidated location, and update the one test that still asserted
against the bare literal instead of the class's own property.

No gameplay numbers moved — `AttackRangeUnits` defaults to the exact same `1400.0f`
`GetAttackRangeUnits()` already returned. This is a pure value-source change
(literal → reflected property) plus documentation.

## Verifiable evidence

All snippets below were re-read directly from the live `app/` source at the time
this file was written.

**The consolidated tunable block, `Source/KrowdKontrol/SniperEnemy.h:71-90`:**

```cpp
	// Sniper engagement tunables (issue #362): all 4 first-pass balance numbers
	// introduced by the SN-1PR sniper-shot PRD (#358 damage, #359 telegraph, #360
	// chase speed/attack range) live in this class as EditDefaultsOnly UPROPERTYs -
	// AttackTelegraphSeconds below, plus AttackRangeUnits, ShotDamageAmount, and
	// MovementSpeed further down this file. None of these are final balance
	// decisions; each is called out below with its current value.
	// SN-1PR's first-pass telegraph duration (issue #359) - the countdown between
	// entering Attack and the shot actually firing. Subject to operator playtest
	// tuning.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0.0"))
	float AttackTelegraphSeconds = 1.2f;

	// SN-1PR's first-pass attack-range value (issue #360's design context, formally
	// named here for #362) - deliberately close to DetectionRangeUnits's default
	// (1500.0f, inherited unchanged), so SN-1PR enters Attack almost immediately
	// after Alert without needing to close distance first - the mechanical
	// definition of "long-range" in this state machine. Subject to operator
	// playtest tuning.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sniper", meta = (ClampMin = "0.0"))
	float AttackRangeUnits = 1400.0f;
```

**`GetAttackRangeUnits()` now a pure passthrough, `Source/KrowdKontrol/SniperEnemy.cpp:83-88`:**

```cpp
float ASniperEnemy::GetAttackRangeUnits() const
{
	// See AttackRangeUnits's own comment in SniperEnemy.h for the design rationale
	// (issue #362).
	return AttackRangeUnits;
}
```

**Test `(j)` updated to assert against the property, `Source/KrowdKontrol/Private/Tests/KrowdKontrolSniperEnemyTest.cpp:343-345`:**

```cpp
	ASniperEnemy* LongRangeSniper = NewObject<ASniperEnemy>();
	TestEqual(TEXT("(j) GetAttackRangeUnits() should return the named AttackRangeUnits constant"),
		LongRangeSniper->GetAttackRangeUnits(), LongRangeSniper->AttackRangeUnits);
```

## Current baseline values (for balance review)

All four SN-1PR sniper-engagement tunables, gathered here for a single reference
point:

| Tunable | Property | Current value |
|---------|----------|----------------|
| Telegraph duration | `AttackTelegraphSeconds` | `1.2f` |
| Per-hit damage | `ShotDamageAmount` | `8.0f` |
| Attack range | `AttackRangeUnits` | `1400.0f` |
| Chase speed | `MovementSpeed` | `300.0f` |

None of these are final balance decisions — each carries its own "subject to
operator playtest tuning" comment in `SniperEnemy.h`.

## Acceptance criteria

- [x] All 4 sniper-engagement tunables (`AttackTelegraphSeconds`, `ShotDamageAmount`,
      `AttackRangeUnits`, `MovementSpeed`) are `EditDefaultsOnly` `UPROPERTY`s on
      `ASniperEnemy`, in `SniperEnemy.h` (single file/class)
- [x] Each of the 4 has an inline comment giving its current first-pass value and
      stating it is subject to operator playtest tuning
- [x] This file lists all 4 current baseline values together
- [x] `GetAttackRangeUnits()` returns the new `AttackRangeUnits` property, not a
      literal
- [x] Test `(j)` asserts against the property, not a magic number, matching test
      `(x)`'s existing pattern for `MovementSpeed`
- [x] Every edit to `app/Source/KrowdKontrol/...` has a matching, identical edit
      under `app-source-tracked/Source/KrowdKontrol/...`

## Validation evidence

```
$ python harness/ci.py --quick
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=131
PIE_PASSED tests=8
GATE_OK mode=quick
```

```
$ harness/run_ue_automation.sh KrowdKontrol.Unit.SniperEnemy
UE_BUILD_START KrowdKontrolEditor Win64 Development
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
```

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and
its matching `app-source-tracked/` copy are the tracked-repo record of that change,
per D-009. Not a substitute for reading `app-source-tracked/` directly.
