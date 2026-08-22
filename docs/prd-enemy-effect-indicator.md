# PRD: Enemy Effect Indicator (which CC effect, how long left)

**Author**: operator (Severin), drafted with the interactive session, 2026-08-22.
**Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md's colour-coded
crowd-control identity (`02` — the five reserved ability colours ARE the game's
information system) and the 2026-08-22 operator playtest: a stunned enemy freezes
with no indication of *what* is affecting it or *how long it will last*, so the
player can't plan around control windows — the exact skill the whole game is
about (the playtest's chase-freeze-chase Runner confusion came directly from an
invisible stun timer).

## Problem

When an enemy is Controlled, nothing communicates the effect. The player cannot
see which ability is holding it or when it will break free. All the data exists —
`AEnemyBase` tracks `ControllingAbility` and `RemainingControlledSeconds`
(including per-enemy overrides like Sleep-vs-Sniper's 7s) — but it is private
state with no visual surface.

## Requirements

### REQ-1: World-space effect indicator on Controlled enemies (P0)
- While an enemy's state is `Controlled`, show a small world-space indicator at
  the enemy: a **depleting progress bar** for the remaining control duration,
  filled in the **controlling ability's reserved colour**
  (`AbilityData::Get(ControllingAbility).Colour` — the colour IS the "which
  effect" answer per MISSION Hard Invariant 3; no text label needed at
  placeholder quality).
- Appears the moment control lands, drains in real time from full to empty over
  the actual duration (overrides included — a 7s Sleep on a Sniper drains over
  7s, not the base duration), and disappears immediately on reversion to Alert
  or on Banked.
- Placeholder-first rendering is fine (widget component or simple mesh/material
  bar), mirroring the existing enemy-type indicator's world-space pattern.

### REQ-2: Expose the duration state cleanly (P0)
- `AEnemyBase` gains public read accessors for remaining and total control
  duration (e.g. `GetRemainingControlledSeconds()` /
  `GetTotalControlledSeconds()`), guarded by the same "only meaningful while
  Controlled" contract `GetControllingAbility()` documents. The indicator reads
  these; nothing else touches the private fields.
- Automation tests: fraction is 1.0 at control, decreases under
  `TickControlledDuration`, reflects `GetControlledDurationOverrideSeconds`
  overrides, and the accessors' stale-read contract holds after expiry/bank.

### REQ-3: Readable at gameplay scale and in crowds (P1)
- Legible from the game camera's real framing (the #188 tunable defaults), and
  sane when several controlled enemies stand near each other (a herded train,
  once #214 lands): bars stay attached to their enemy, no overlap chaos, no
  z-fighting with the enemy-type indicator text — position them as siblings,
  not stacked at the same offset.
- Automation tests for what is testable headlessly (indicator exists/visible
  flag per state, offset differs from the type indicator's); visual quality is
  playtest-verified.

## Out of scope
- Player-side status indicators (the HUD ability tray already shows cooldowns
  and lockouts; punishments-on-player display is the punishment PRD's tray
  lineage).
- Boss state UI (`ABossBase` shield/enrage phases have their own mechanics and
  deserve their own design pass).
- Damage numbers, health bars over enemies (enemies have no health — non-lethal
  by Hard Invariant), or any effect other than the Controlled state.
- Icons/text per ability — the reserved colour carries the information at this
  stage; iconography is a later art pass.

## Existing surfaces to build on (do not reinvent)
`AEnemyBase`: `ControllingAbility` / `GetControllingAbility()`,
`RemainingControlledSeconds` + `TickControlledDuration` +
`GetControlledDurationOverrideSeconds`, `OnControlledEntry`,
`OnEnemyControlledExpired`, `OnEnemyBanked`;
`AbilityData::Get(...).Colour` + `ReservedGameplayColours` (Hard Invariant 3);
the enemy-type indicator component (existing world-space-on-enemy precedent,
including its mirrored-text lesson from issue #134); the camera framing
defaults from issue #188 for legibility judgment.
