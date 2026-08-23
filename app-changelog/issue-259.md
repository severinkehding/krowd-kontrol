# Issue #259: Ability tray cooldown countdown fill + numeric readout

`UAbilityCooldownTrayWidget` previously ran an entirely self-contained placeholder
cooldown timer (`SeedPlaceholderCooldowns()`/`StartCooldown()`/`AdvanceCooldowns()`),
never touching the real `UAbilityCooldownComponent` that `UAbilityCastComponent`
actually gates real casts through, and had no visual fill at all - only a numeric text
readout. The HUD countdown a player saw was disconnected from real gameplay cooldown
state.

Per the issue's OWNER-authored reopening comment, this PR adds the missing
start/expire delegate surface to `UAbilityCooldownComponent` itself (it was
deliberately poll-only until now) in the same PR as the tray wiring, rather than
splitting it into a separate issue. `TryStartCooldown` remains the component's only
public mutator - the new `OnAbilityCooldownChanged` is a `BlueprintAssignable`
delegate *property*, not a mutator, and is only ever broadcast from inside the two
functions (`TryStartCooldown`/`AdvanceCooldowns`) that already own all state changes.

The tray widget now binds to that delegate (`BindAbilityCooldownComponent`, mirroring
`BindAbilityLockoutComponent`'s weak-ref/rebind-safe/`AddUniqueDynamic` shape), and
once bound, switches its per-frame update from the self-decrementing placeholder timer
to polling the real component's live `GetRemainingCooldownSeconds()`
(`RefreshCooldownReadouts()`, mirroring `RefreshPunishmentLockoutReadouts()`). A new
per-slot `UProgressBar` (`BarFillType::TopToBottom`, no new Content asset - vertical
drain was chosen over a radial fill, which would require a new UMG material `.uasset`,
conflicting with this widget's C++-only, no-Content-asset construction) renders the
countdown visually, and a manual per-frame colour tween (mirroring
`UEnergyMeterWidget`'s damage-flash pattern - `UWidgetAnimation` isn't usable from
pure C++, and this widget has no Widget Blueprint asset to host one) plays a brief
brightness "ready-flash" pulse on the icon border when a cooldown clears.

The placeholder timer path (`AdvanceCooldowns()`/`SeedPlaceholderCooldowns()`) is kept
unchanged for any tray instance that is never bound to a real component - it also now
plays the same ready-flash on its own natural expiry, so the flash is consistent
regardless of bound-vs-placeholder mode.

## Files changed

The real Unreal project lives under `app/` (gitignored symlink, D-003) and is what the
harness actually builds/tests against - `app/` itself is unchanged by this PR's
tracking. What this PR's diff actually contains is a **copy** of the changed source,
per D-009, at `app-source-tracked/<same path under app/Source/>`.

| File (under `app-source-tracked/Source/KrowdKontrol/`) | Action | What it contains |
|------|--------|-------------------|
| `AbilityCooldownComponent.h` | UPDATE | New `FOnAbilityCooldownChanged(EAbilitySlot, bool)` dynamic-multicast delegate declaration + `OnAbilityCooldownChanged` `BlueprintAssignable` property; new `friend class FKrowdKontrolAbilityCooldownTrayWidgetTest`; class comment updated to state the delegate does not relax the "no new public mutator" invariant |
| `AbilityCooldownComponent.cpp` | UPDATE | `TryStartCooldown` broadcasts `(Ability, true)` only on the `<=0 -> >0` transition (guarded so a misconfigured/clamped-to-0 duration never broadcasts "started"); `AdvanceCooldowns` broadcasts `(Ability, false)` only on the `>0 -> <=0` transition, exactly once per crossing |
| `AbilityCooldownTrayWidget.h` | UPDATE | New public `BindAbilityCooldownComponent()`/`RefreshCooldownReadouts()`/`GetSlotCooldownFillFraction()`/`IsSlotReadyFlashActive()`/`GetSlotReadyFlashRemainingSeconds()`; new private `HandleAbilityCooldownChanged`/`PlayReadyFlash`/`AdvanceReadyFlashTimers`; new state (`SlotCooldownFillBars`, `SlotReadyFlashRemaining`, `BoundCooldownComponent`) |
| `AbilityCooldownTrayWidget.cpp` | UPDATE | `BuildWidgetTree()` constructs a `UProgressBar` (wrapped in a `USizeBox` for sizing) per slot, layered between the icon border and cooldown text; `UpdateSlotVisual()` gains a ready-flash border-colour branch (highest priority) and a fill-percent/visibility block that runs on every branch (locked states hide it); `NativeTick()` branches bound-vs-placeholder; `StartCooldown()` clears any stale ready-flash on recast; placeholder `AdvanceCooldowns()` now also plays the ready-flash on natural expiry; new `BindAbilityCooldownComponent`/`HandleAbilityCooldownChanged`/`RefreshCooldownReadouts`/`PlayReadyFlash`/`AdvanceReadyFlashTimers` and the three new accessors |
| `KrowdKontrolPlayerController.cpp` | UPDATE | `WireWidgetsToPawn()` adds a third bind call, `AbilityTrayWidget->BindAbilityCooldownComponent(InPawn->FindComponentByClass<UAbilityCooldownComponent>())` - real production wiring, since `AFlatCamera3DPrototypePawn` already has an `AbilityCooldownComponent` subobject that `UAbilityCastComponent` already gates real casts through |
| `Private/Tests/KrowdKontrolAbilityCooldownTest.cpp` | UPDATE | New block (h): asserts `OnAbilityCooldownChanged` broadcasts true exactly once on start, does not re-broadcast on a blocked recast, broadcasts false exactly once at expiry, and does not re-broadcast on an already-expired advance |
| `Private/Tests/KrowdKontrolAbilityCooldownTrayWidgetTest.cpp` | UPDATE | New block (o): binds a real `UAbilityCooldownComponent`, asserts fill fraction + numeric readout at start (1.0) and partway through a friend-access partial advance, then asserts both clear to 0/empty and the tile reports `Ready` + an active ready-flash at expiry; new block (p): null-guard on `BindAbilityCooldownComponent(nullptr)` |
| `Private/Tests/AbilityCooldownChangedTestListener.h` | CREATE | `UFUNCTION()`-based listener object for `OnAbilityCooldownChanged` - dynamic multicast delegates don't support `AddLambda`, mirrors `AbilityLockoutChangedTestListener`'s identical shape |
| `Private/Tests/AbilityCooldownChangedTestListener.cpp` | CREATE | Implementation - counts true/false broadcasts and records the last ability slot |

No `.Build.cs` change - no new module dependencies (`Components/ProgressBar.h` and
`Components/SizeBox.h` are already used elsewhere in this module).

## Acceptance criteria

- [x] Tile gains a visual countdown fill reflecting remaining cooldown fraction -
      `UProgressBar` + `UpdateSlotVisual`'s fill block, proven by
      `GetSlotCooldownFillFraction()` assertions in block (o)
- [x] Tile shows a numeric seconds-remaining readout sourced from
      `UAbilityCooldownComponent` - proven by `GetSlotCooldownDisplayText()`
      assertions in block (o)
- [x] Start/stop is event-driven, bound to the cooldown component's start/expire
      delegates, matching `SetSlotLocked`/`BindAbilityUnlockComponent`'s precedent
- [x] Fill ticks every frame once started, via `RefreshCooldownReadouts()` called from
      `NativeTick()` when bound
- [x] On expiry, tile returns to full ready brightness with a brief ready-flash pulse -
      proven by `IsSlotReadyFlashActive()`/`GetSlotReadyFlashRemainingSeconds()`
      assertions in block (o)
- [x] Automation test(s) in `KrowdKontrol.Unit.` seed a real cooldown, assert
      fill+text at start and partway, then assert both clear and ready state at/after
      expiry - block (o)
- [x] Does not reinvent cooldown truth - `RefreshCooldownReadouts()` always reads live
      from `BoundCooldownComponent`; the widget's own `SlotCooldownRemaining` is a
      cache refreshed from the component, never an independent timer, once bound
- [x] Implementer's choice for the fill visual: **vertical drain**
      (`UProgressBar`, `BarFillType::TopToBottom`). Radial rejected - would require a
      new UMG material `.uasset`, conflicting with this widget's deliberate C++-only,
      no-Content-asset construction and this repo's `app-source-tracked/` PR-mirror
      exclusion of binary assets.

## Validation evidence

```
HARNESS_START mode=quick driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=99
GATE_OK mode=quick
```

Targeted run before the full suite:

```
$ harness/run_ue_automation.sh KrowdKontrol.Unit.AbilityCooldown
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=2 total=2
UE_AUTOMATION_OK
```

(`KrowdKontrol.Unit.AbilityCooldown` and `KrowdKontrol.Unit.AbilityCooldownTrayWidget`
both match this prefix filter.)

Full suite (`harness/run_ue_automation.sh KrowdKontrol.Unit.`) passed 99/99, and
`KrowdKontrol.Smoke.` passed 1/1 - both clean, no regressions elsewhere (including
`KrowdKontrolHUDWiringTest`, which was checked against this issue's plan and does not
enumerate `WireWidgetsToPawn()`'s individual bind calls, so it needed no change).

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and its
matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
