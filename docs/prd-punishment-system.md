# PRD: Punishment System (Punishments 1 & 2 + arbitration)

**Author**: operator (Severin), drafted with the interactive session, 2026-08-19.
**Feeds**: `dark-factory-prd-to-issues`. Grounded in MISSION.md (`08`): "the existing
ability-lock (Punishment 1) and run-speed-reduction (Punishment 2) mechanics, with
only one punishment state active at a time" — alongside the already-merged Overcrowd
punishment (Punishment 3: detection PR #124, thresholds PR #147, recovery PR #153,
audio muffling PR #142, screen distortion in review). Closed issues #24 and #26 were
rejected solely because Punishments 1 and 2 did not exist; this PRD makes them exist.

## Problem

Only Punishment 3 (Overcrowd/Panic Overload) is implemented. Punishment 1's *visual*
(the ability tray's locked-slot state, PR #129) merged with an explicit "no real
lockout gameplay logic exists yet — placeholder driver only" note. Punishment 2 has
nothing at all. The single-active-punishment arbitration rule cannot exist without at
least two punishments to arbitrate.

## Requirements

### REQ-1: Punishment trigger condition (P0)
Define the shared trigger both new punishments respond to, per PRD 08's design as
summarized in MISSION: punishments fire on player misplay. Concretely for this
codebase today: **taking enemy contact damage** (`UPlayerEnergyComponent::
ApplyContactDamage` already fires for Bomber explosions and Trooper/Sniper attacks)
is the trigger event. A punishment manager (component on the pawn or world subsystem)
decides which punishment activates per REQ-4's arbitration.

### REQ-2: Punishment 1 — ability lockout (P0)
- On activation: the player's most recently cast ability (fallback: Stun) becomes
  uncastable for a fixed lockout duration (config default, e.g. 8s — distinct from and
  longer than its normal cooldown, per the merged tray-visual's design notes).
- `UAbilityCastComponent::TryCastAbility` must gate on lockout the same way it gates
  on unlock/cooldown (new check, same pattern).
- The already-merged tray locked-slot visual (`SetSlotLocked`, PR #129) is driven by
  the real lockout state — replacing its "placeholder driver only" status. Slot shows
  locked while locked out, reverts on expiry.
- Automation tests: lockout blocks casting; expiry restores it; tray state mirrors.

### REQ-3: Punishment 2 — run-speed reduction (P0)
- On activation: the player pawn's movement speed is reduced by a configurable factor
  (e.g. 50%) for a fixed duration, then restores. Both prototype pawns use direct
  input-driven movement — implement the modifier where movement input is applied so
  it works for both.
- Automation tests: speed factor applies on activation, restores on expiry, does not
  stack with itself.

### REQ-4: Single-active-punishment arbitration (P1)
Closed issue #24's ask: at most one punishment state active at any time, priority
Overcrowd (3) > ability-lock (1) > speed-reduction (2). A higher-priority activation
preempts a lower one (lower ends immediately); a lower-priority trigger while a higher
is active is dropped. The Overcrowd state is read from the existing
`UOvercrowdDetectionComponent`. Automation tests cover each preemption/drop pairing.

### REQ-5: Per-punishment debug toggles (P2)
Closed issue #26's ask, minimally: config-driven (ini/CVar) enable/disable per
punishment so playtests can isolate mechanics. No menu UI required — CVars are enough
at this stage.

## Out of scope
- Overcrowd itself (merged) beyond arbitration integration.
- Difficulty scaling of punishment parameters per level (later balancing pass).
- Any lethal consequence — punishments never kill (MISSION Hard Invariant 2).

## Existing surfaces to build on (do not reinvent)
`UPlayerEnergyComponent::ApplyContactDamage`; `UAbilityCastComponent::TryCastAbility`
gate chain; `UAbilityCooldownTrayWidget::SetSlotLocked` (merged Punishment-1 visual);
`UOvercrowdDetectionComponent` (Punishment 3 state); pawn movement input paths in
`AFlatCamera3DPrototypePawn` / `APaper2DPrototypePawn`.
