# PRD: The Herd Mechanic (closing the P0 core loop)

**Author**: operator (Severin), drafted with the interactive session, 2026-08-22.
**Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md's P0 core loop (`01`):
perceive → prioritize → control → **herd (spatially guide a controlled enemy to a
target zone)** → bank → escalate → clear. Every step of that loop except *herd* has
merged code behind it; herd has no issue, no code, and no design anywhere in the
repo — found during the 2026-08-22 live operator verification that also produced
open issue #211 (the bank-delivery chain: `IHerdable` on `AEnemyBase`, `ATargetZone`
placement, `OnActorBanked` → `TransitionToBanked` wiring). #211 is already open and
is NOT part of this PRD's decomposition — do not duplicate it; this PRD supplies the
missing movement half that #211's chain delivers into.

## Problem

`Controlled` enemies are immobile (`TickChaseMovement` is Alert-only), so even with
#211's chain landed, the player would have no way to move a controlled enemy to a
zone. The game cannot be completed by any real playthrough until this exists —
verified live: level-clear, clear-time saves (#205/#154's long-missing
`Saved/SaveGames/` evidence), the post-run summary, and run-complete are all
unreachable outside automation tests.

## Operator design decision (2026-08-22, locked — do not re-litigate at triage)

**Controlled enemies follow the player**, pied-piper style: while
`CurrentState == Controlled`, an enemy moves toward the player pawn at a fixed
follow speed, trailing behind. The player herds by walking — no new input.
Considered and rejected by the operator: auto-walk to zones (removes the herding
skill; cast would be the whole loop), physical pushing (feel risk with direct
top-down movement), click-to-send (introduces a new input paradigm alongside
WASD+abilities).

## Requirements

### REQ-1: Follow-the-player movement while Controlled (P0) — ✅ implemented, issue #214
- While `Controlled`, an enemy moves toward the player pawn each tick, clamped
  `min(remaining, speed × dt)` per the existing `TickChaseMovement` convention.
- Stops at a small follow distance so enemies trail behind rather than stack on
  the pawn; a group of controlled enemies forms a loose train.
- Follow speed is configurable (`EditDefaultsOnly`), defaulting slower than chase
  speed — herding a crowd should have weight; exact value is the implementer's
  judgment, stated in the PR.
- Follow ends when Controlled ends (duration reversion to Alert, or Banked) —
  existing transitions unchanged.
- Automation tests: moves toward player only while Controlled; stops at follow
  distance; Idle/Alert/Attack/Banked unaffected; duration-reversion mid-follow
  reverts cleanly; elite speed multiplier interaction stated and tested.

### REQ-2: Herding feels readable at the crowd scale (P1)
- Multiple controlled enemies following simultaneously must not visually merge
  into one blob: minimal separation between followers (placeholder-quality —
  simple radial offset or per-follower slot is fine; no flocking system).
- Crowd Mastery (`UCrowdMasterySubsystem`, merged) already samples on cast — a
  larger sustained train is the skill this makes real; no new tracking required.

### REQ-3: End-to-end winnability proof (P0 — acceptance, depends on #211)
Once this PRD's REQ-1 and open issue #211 have both merged: a real PIE playthrough
of L_Level01 — casting on enemies, walking them onto colour-matched target zones —
fires `OnLevelClear` and produces a clear-time save file in `Saved/SaveGames/`.
That exact evidence has been waiting through #154's E2E and #205's AC #5; it is
the acceptance test for the loop being closed, and belongs to whichever of the
two (this PRD's implementation or #211) lands second.

## Out of scope
- #211's chain itself (already an open issue — dedupe, don't duplicate).
- Obstacle-aware approach routing (closed issue #83 — reopens after the chain
  exists).
- Any flocking/avoidance system beyond REQ-2's minimal separation.
- Herding for bosses (`ABossBase` never self-aggros and banks via its own
  mechanics).

## Existing surfaces to build on (do not reinvent)
`AEnemyBase::TickChaseMovement` (movement-clamp pattern) / `TickControlledDuration`
/ `GetMovementSpeedUnitsPerSecond` + elite multiplier;
`UGameplayStatics::GetPlayerPawn` lookup in `AEnemyBase::Tick`;
`UCrowdMasterySubsystem`; open issue #211's chain as the delivery target.
