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
| `AbilityCooldownComponent.h` | UPDATE | New `FOnAbilityCooldownChanged(EAbilitySlot, bool)` dynamic-multicast delegate declaration + `OnAbilityCooldownChanged` `BlueprintAssignable` property; new `friend class FKrowdKontrolAbilityCooldownTrayWidgetTest` and `friend class FKrowdKontrolReservedGameplayColoursTest` (the latter added in review response, so the colour audit can drive a real cooldown to expiry); class comment updated to state the delegate does not relax the "no new public mutator" invariant |
| `AbilityCooldownComponent.cpp` | UPDATE | `TryStartCooldown` broadcasts `(Ability, true)` only on the `<=0 -> >0` transition (guarded so a misconfigured/clamped-to-0 duration never broadcasts "started"); `AdvanceCooldowns` broadcasts `(Ability, false)` only on the `>0 -> <=0` transition, exactly once per crossing |
| `AbilityCooldownTrayWidget.h` | UPDATE | New public `BindAbilityCooldownComponent()`/`RefreshCooldownReadouts()`/`GetSlotCooldownFillFraction()`/`IsSlotReadyFlashActive()`/`GetSlotReadyFlashRemainingSeconds()`; new private `HandleAbilityCooldownChanged`/`PlayReadyFlash`/`AdvanceReadyFlashTimers`; new state (`SlotCooldownFillBars`, `SlotReadyFlashRemaining`, `BoundCooldownComponent`) |
| `AbilityCooldownTrayWidget.cpp` | UPDATE | `BuildWidgetTree()` constructs a `UProgressBar` (wrapped in a `USizeBox` for sizing) per slot, layered between the icon border and cooldown text; `UpdateSlotVisual()` gains a ready-flash border-colour branch (highest priority) and a fill-percent/visibility block that runs on every branch (locked states hide it); `NativeTick()` branches bound-vs-placeholder; `StartCooldown()` clears any stale ready-flash on recast; placeholder `AdvanceCooldowns()` now also plays the ready-flash on natural expiry; new `BindAbilityCooldownComponent`/`HandleAbilityCooldownChanged`/`RefreshCooldownReadouts`/`PlayReadyFlash`/`AdvanceReadyFlashTimers` and the three new accessors |
| `KrowdKontrolPlayerController.cpp` | UPDATE | `WireWidgetsToPawn()` adds a third bind call, `AbilityTrayWidget->BindAbilityCooldownComponent(InPawn->FindComponentByClass<UAbilityCooldownComponent>())` - real production wiring, since `AFlatCamera3DPrototypePawn` already has an `AbilityCooldownComponent` subobject that `UAbilityCastComponent` already gates real casts through |
| `Private/Tests/KrowdKontrolAbilityCooldownTest.cpp` | UPDATE | New block (h): asserts `OnAbilityCooldownChanged` broadcasts true exactly once on start, does not re-broadcast on a blocked recast, broadcasts false exactly once at expiry, and does not re-broadcast on an already-expired advance. Review response: block (h) also now asserts a clamped-to-0 configured duration does not broadcast true *with the listener actually attached* to observe it |
| `Private/Tests/KrowdKontrolAbilityCooldownTrayWidgetTest.cpp` | UPDATE | New block (o): binds a real `UAbilityCooldownComponent`, asserts fill fraction + numeric readout at start (1.0) and partway through a friend-access partial advance, then asserts both clear to 0/empty and the tile reports `Ready` + an active ready-flash at expiry; new block (p): null-guard on `BindAbilityCooldownComponent(nullptr)`. Review response: block (f4) also asserts the fill fraction is hidden (0) while locked; block (o) also drives a real `NativeTick()` call to prove its bound-component dispatch branch, and a recast-during-flash sequence to prove the stale-flash-clearing guard; new block (o2) proves the placeholder-mode ready-flash fires on natural expiry too, mirroring block (o)'s bound-mode assertion |
| `Private/Tests/AbilityCooldownChangedTestListener.h` | CREATE | `UFUNCTION()`-based listener object for `OnAbilityCooldownChanged` - dynamic multicast delegates don't support `AddLambda`, mirrors `AbilityLockoutChangedTestListener`'s identical shape |
| `Private/Tests/AbilityCooldownChangedTestListener.cpp` | CREATE | Implementation - counts true/false broadcasts and records the last ability slot |
| `Private/Tests/KrowdKontrolReservedGameplayColoursTest.cpp` | UPDATE (review response) | New (2c) sub-case in the tray audit: binds a real `UAbilityCooldownComponent`, drives each slot through an active cooldown and a natural expiry, and asserts neither the fill-bar colour nor the ready-flash border colour collides with a reserved gameplay colour - the two new colour paths this PR introduces weren't previously run through this audit |

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

## Review response

The original commit (`116b196`) had unrelated issue #260 (hover tooltip) code bundled
into `AbilityCooldownTrayWidget.h`/`.cpp` - an `#include "AbilityTooltipWidget.h"`, a
`SlotTooltip` construction block in `BuildWidgetTree()`, and a
`friend class FKrowdKontrolAbilityTooltipWidgetTest;` - none of it mentioned in this
issue's scope, and referencing a type (`UAbilityTooltipWidget`) that doesn't exist
anywhere in this PR's own diff or in `main`. Root cause: `app/` is a gitignored symlink
shared across concurrently-dispatched factory tasks (D-003) - issue #260's task had
uncommitted edits sitting in that shared tree when this task's commit step ran, and they
got swept in indiscriminately. Fixed by removing all three pieces from both `app/` and
this tracked mirror; issue #260's actual work is unaffected - it already has its own
commits on `origin/archon/task-fix-issue-260` and its own PR.

The four test-coverage gaps flagged in review (new fill/flash colours never run through
the reserved-colour audit; placeholder-mode ready-flash untested; the recast-during-
flash stale-flash-clear guard untested; `NativeTick()`'s bound-component dispatch branch
never exercised through `NativeTick()` itself) and the two lower-priority gaps (fill
hidden while locked; clamped-to-0 duration with a listener attached) were all fixed -
see the `Files changed` table above for exactly which blocks changed.

## Validation evidence

Rebuilt and re-ran after the review-response fixes above (with issue #260's still-stray,
uncommitted `KrowdKontrolAbilityTooltipWidgetTest.cpp` temporarily moved out of the
shared `app/` tree so this PR's own code could be verified in isolation, then moved back
unchanged - it isn't part of this PR's diff and its own branch/PR owns it):

```
$ harness/run_ue_automation.sh KrowdKontrol.Unit.AbilityCooldown
UE_BUILD_OK
UE_AUTOMATION_RESULT passed=2 total=2
UE_AUTOMATION_OK

$ harness/run_ue_automation.sh KrowdKontrol.Unit.ReservedGameplayColours
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK

$ harness/run_ue_automation.sh KrowdKontrol.Unit.
UE_AUTOMATION_RESULT passed=98 total=98
UE_AUTOMATION_OK
```

98/98 (not 99/99 as originally claimed) is correct and expected: the original 99 count
included issue #260's leaked `KrowdKontrolAbilityTooltipWidgetTest`, which was never
this PR's own test and is excluded here along with the rest of that code. No regressions
elsewhere.

---

The real Unreal project stays under `app/` (gitignored, D-003) - this changelog and its
matching `app-source-tracked/` copy are the tracked-repo record of that change, per
D-009. Not a substitute for reading `app-source-tracked/` directly.
