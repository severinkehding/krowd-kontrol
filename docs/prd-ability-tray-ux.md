# PRD: Ability Tray UX (cooldown visibility, tooltips, state clarity)

**Author**: operator (Severin), 2026-08-22, from the live co-op playtests.
**Feeds**: `dark-factory-prd-to-issues`.

## Problem

The ability tray shows five tiles and nothing else. During play the operator
had no way to know: when an ability comes off cooldown (no timer, no visual
countdown), what an ability even does (no hover tooltip), or why a tile is
unavailable (cooldown vs punishment-lockout vs not-yet-unlocked all look
near-identical). Every one of those questions had to be answered over chat by
a human reading the logs.

## Requirements

### REQ-1: Cooldown countdown on the tile (P0)
- While an ability is on cooldown, its tile shows a **visual countdown fill**
  (radial sweep or vertical drain — implementer's judgment, stated in the PR)
  plus a **numeric seconds remaining** readout, both driven by the real
  `UAbilityCooldownComponent` state.
- Event-driven start/stop per the HUD convention; the fill itself may tick on
  the widget side.
- Ready state is visually unmistakable (tile returns to full brightness; a
  brief ready-flash is welcome, placeholder-quality).
- Automation tests: fill/number reflect a seeded cooldown; clears on expiry.

### REQ-2: Hover tooltips (P0)
- Hovering a tile with the (new, in-game) cursor shows a compact tooltip:
  ability name, current key binding(s), one-line effect description, duration,
  range/shape, and its colour-matched enemy type with the reserved-colour
  swatch — all sourced from `AbilityData`, no hardcoded strings per tile.
- Depends on the Cursor & Aiming Foundation PRD's cursor issue (a tooltip
  needs a pointer); decompose with that as a named prerequisite.
- Tooltip chrome obeys `HUDChromeColours` (Hard Invariant 3 — the swatch is
  the only reserved-colour use).

### REQ-3: Unambiguous tile states (P1)
- The four unavailable-ish states get visually distinct treatments:
  **cooldown** (countdown fill), **punishment lockout** (the existing locked
  visual plus its own remaining-time readout from `UAbilityLockoutComponent`),
  **not yet unlocked** (existing locked style, no timer), **ready** (bright).
- Automation tests: each state drives its distinct visual flag.

## Out of scope
- Tray reordering/customization.
- The enemy-side effect-duration bar (open issue #225 — already queued).
- Controller focus navigation of the tray.
- Final art; placeholder-quality visuals throughout.

## Existing surfaces to build on (do not reinvent)
`UAbilityCooldownTrayWidget` (+ `SetSlotLocked`, `BindAbilityUnlockComponent`);
`UAbilityCooldownComponent` (the only cooldown truth) and
`UAbilityLockoutComponent` (lockout truth); `AbilityData` (all tooltip
content); `HUDChromeColours`; the Cursor & Aiming PRD's cursor for hover.
