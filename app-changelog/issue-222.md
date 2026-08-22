# Issue #222: Same-frame damage-flash reaction on the energy meter HUD

Adds a placeholder-quality "damage flash" reaction to `UEnergyMeterWidget` (the HUD
half of PRD "Level Progression & Teaching Arc" REQ-4). `UPlayerEnergyComponent::OnEnergyChanged`
already broadcasts synchronously, same-frame, off `ApplyContactDamage()`, and the
widget's `HandleEnergyChanged()` already updated the fill bar/text in response — but
nothing drew the eye to the change, so a hit didn't visually *read* as landing even
though the underlying state update was instant. A new hidden-by-default `UBorder`
overlay child (`DamageFlashOverlay`), stacked on top of the meter's existing
fill/text via the same `UOverlay`, becomes briefly visible the instant
`HandleEnergyChanged()` fires and auto-hides again after `DamageFlashDurationSeconds`
(0.15s), driven by `NativeTick()` decrementing a plain countdown float — the same
shape `UOnScreenPromptWidget::AdvanceDismissTimer()` already established for this
codebase's widget layer, not a new `FTimerHandle` (which has no widget-layer
precedent here).

## Files changed (all under `app/`, gitignored per D-003 — this is the tracked-repo
record of that change, see the closing note below)

| File | Action | What it contains |
|------|--------|-------------------|
| `app/Source/KrowdKontrol/EnergyMeterWidget.h` | UPDATE | `DamageFlashOverlay` member, `NativeTick()` override, `AdvanceDamageFlashTimer()`/`IsDamageFlashActive()` public API, `PlayDamageFlash()`/`ClearDamageFlash()` private helpers, `DamageFlashRemainingSeconds`/`DamageFlashDurationSeconds` state. |
| `app/Source/KrowdKontrol/EnergyMeterWidget.cpp` | UPDATE | Constructs `DamageFlashOverlay` (hidden, `Collapsed`) in `BuildWidgetTree()`; `HandleEnergyChanged()` now also calls `PlayDamageFlash()`; `NativeTick()` calls `AdvanceDamageFlashTimer()` every frame, which decrements the countdown and calls `ClearDamageFlash()` once it hits zero. |
| `app/Source/KrowdKontrol/Private/Tests/KrowdKontrolEnergyMeterWidgetTest.cpp` | UPDATE | New check (7b), attached immediately after the existing (7) live `ApplyContactDamage()`-driven assertion: flash activates synchronously off the same broadcast, flash colour doesn't collide with a reserved gameplay colour, `AdvanceDamageFlashTimer()` clears the flash after its duration, and a direct `SetEnergy()` call does not itself trigger the flash. Extended by the pass-1 fix pass (commit 0712205) with three more assertions: sampling `AdvanceDamageFlashTimer()` mid-flight at half duration (not just the two endpoints), check (7c) confirming a re-trigger mid-countdown resets to full duration rather than accumulating/being ignored, and extending check (12) to bind a real `UPlayerEnergyComponent` and fire `ApplyContactDamage()` on the unbuilt widget to exercise `PlayDamageFlash()`'s null-`DamageFlashOverlay` guard. `NativeTick()` itself remains unexercised by an Automation test (accepted precedent, headless `-nullrhi`; tracked as a separate PIE-driven Smoke-tier follow-up). |
| `app/Source/KrowdKontrol/EnergyMeterWidget.h` / `.cpp` | UPDATE (pass-1 fix pass) | Added `GetDamageFlashRemainingSeconds()` (`BlueprintPure`) so the countdown itself, not just the active/inactive bool, is reflected for structured before/after reads. |
| `app/Source/KrowdKontrol/PlayerEnergyComponent.h` | UPDATE (pass-1 fix pass) | `GetCurrentEnergy()` is now `UFUNCTION(BlueprintPure)` (was a plain C++ accessor), so it's reflected the same way. |

## Acceptance criteria

- [x] **`UEnergyMeterWidget` performs an immediate flash/pulse reaction on the same
      frame `OnEnergyChanged` fires with a decrease.** `HandleEnergyChanged()` calls
      `PlayDamageFlash()`, which sets `DamageFlashOverlay` visible synchronously — no
      deferred/next-frame path involved.
- [x] **No change to energy calculation, drain rate, or drain logic.**
      `PlayerEnergyComponent.cpp` is untouched by this change.
- [x] **Automation test confirms binding to `OnEnergyChanged` and firing it
      synchronously triggers the reaction hook on the same frame.** Test (7b),
      attached directly to (7)'s existing `ApplyContactDamage()` call.
- [x] **Visual quality itself is not asserted by the automation test.** The test only
      checks `IsDamageFlashActive()`/`GetVisibility()`/colour non-collision, never
      pixel output or timing "feel."
- [x] **Level 1-3 validation commands pass with exit 0.** See validation evidence
      below.
- [x] **Code mirrors `UOnScreenPromptWidget`'s existing countdown/`NativeTick()`
      pattern exactly.** No new `FTimerHandle` introduced to the widget layer.
- [x] **No regressions in the existing 13 numbered checks in
      `KrowdKontrolEnergyMeterWidgetTest.cpp`.** New check (7b) inserted between (7)
      and (8) without altering either.

## Validation evidence

Per PR #232 pass-1 fix commit (0712205): verified via `harness/ci.py` full mode,
`KrowdKontrol.Unit.EnergyMeterWidget` passes with all new assertions (mid-flight
sample, re-trigger reset, null-overlay guard). One unrelated
`GizmoFirstContactComponent` flake did not reproduce on rerun (transient failure,
unrelated to this change).

## Governance note: `DamageFlashColor` and the 5-colour lock (open, pass-1 security finding)

Pass-1 security review flagged `DamageFlashColor` (`FLinearColor(0.95, 0.12, 0.12,
1.0)`, saturated red) as a new informational-purpose HUD colour and asked for
explicit human product-owner confirmation before merge that it does not amount to
introducing a 6th saturated information colour under MISSION.md Hard Invariant 3.

The PR's position (also now stated inline at the colour's declaration in
`EnergyMeterWidget.cpp`): the five reserved colours (Purple/Snare, Teal/Root,
Orange/Fear, Blue/Sleep, White/Stun) identify a specific ability or enemy type: this
flash identifies nothing — it's a generic, momentary "you were hit" reaction with no
persistent or type-specific meaning, closer to a screen-shake than to a gameplay-
information colour, and it does not visually collide with any of the five (nearest is
Orange, well separated by the green channel).

This is a defensible reading but has not had explicit human sign-off. **This fix pass
could not manufacture that confirmation** (it requires an actual product-owner
decision, not a code change) — flagging it here explicitly so a human reviewer can
approve or reject the specific claim above before merge, rather than fixing it
silently.

## Closing note on `app-source-tracked/`

`app/` itself is a gitignored symlink to the real Unreal project on the Windows host
(D-003) — binary assets can't live in this repo without LFS set up ahead of time.
`app-source-tracked/` mirrors the plain-text `.h`/`.cpp` files touched by this issue
into the tracked repo (D-009) so this PR carries real, reviewable source instead of
just a description of it. `app/` stays exactly as-is; this is a copy for review, not a
new live link.
