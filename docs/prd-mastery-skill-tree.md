# PRD: Crowd Mastery Skill Tree — Spend the Points (P-Organ Style)

**Author**: operator (Severin), 2026-08-28, with two reference screenshots
from Lies of P's P-Organ menu supplied in the design conversation and the
reference page https://liesofp.wiki.fextralife.com/P-Organ.
**Feeds**: `dark-factory-prd-to-issues`. Builds directly on the shipped
mastery stack: GameInstance total (#327), menu display (#328), reset (#329),
cross-launch persistence (#330), and the main menu (#323–#325). Supersedes
the mastery PRD's "no skill tree yet" line — that PRD's display-only scope
shipped; this is the deliberate next step.

## Problem

Crowd Mastery accumulates, displays, persists, and resets — but buys nothing.
The meta loop needs a spend: a skill tree reachable from the main menu where
mastery points unlock skills.

## Operator design decision (2026-08-28, locked — do not re-litigate at triage)

The structure mirrors Lies of P's P-Organ (operator-supplied screenshots):

- **A visual tree of node clusters**: large circular nodes arranged as a
  branching tree across progression phases; around each circle sit **4
  smaller bubbles**. Clicking a bubble **spends mastery points to unlock that
  skill** (one bubble = one skill).
- **Each unlocked skill has 2 modifier slots** (the screenshots' "Empty
  Slot" pair): modifiers are slottable, categorized entries in the reference
  page's style — e.g. "Survival Type Ability Tier I", "Attack Type Ability
  Tier I", "Ability Type Tier I" — organized in tiers (I/II/III) where higher
  tiers unlock as tree phases progress.
- **Reset = full respec**: the existing menu reset (confirm-gated) now also
  clears the tree and refunds every spent point. No partial refunds.
- Reachable as a **menu point from the main menu**; all state lives behind
  the existing `UCrowdMasteryTotalSubsystem` authority + its save-file
  persistence, so a future Steam sync wraps one place.
- Chrome obeys Hard Invariant 3 (neutral chrome; reserved colours only where
  they already mean something).

## Requirements

### REQ-1: Tree data model (P0) — ⚠️ partially implemented, issue #371
- Data-driven definition (DataTable/DataAsset in the established pattern):
  nodes, their 4 skill bubbles, per-bubble point cost, unlock prerequisites
  (parent node reached), phase/tier gating, and the modifier catalog
  (category, tier, effect hook).
- Spend/refund/queries live on the mastery authority (extending
  `UCrowdMasteryTotalSubsystem`): spent points tracked separately from the
  earned total so display and refund stay honest. Unit-tested: spend,
  insufficient-points rejection, prerequisite rejection, full respec refund,
  persistence round-trip.

  Issue #371 shipped the DataTable-driven model, spend/refund/prerequisite
  queries, and their unit tests as session-only state — persistence
  round-trip is explicitly deferred to a follow-up issue (see
  `app-changelog/issue-371.md`).

### REQ-2: Tree screen UI (P0)
- New screen off the main menu ("MASTERY" entry): renders the node/bubble
  tree (C++-built widget tree per the established HUD lineage), current
  points, per-bubble cost and unlocked state, click-to-unlock with an
  affordance for unaffordable/locked bubbles, and a back-to-menu control.
- Visual bar: reads as a tree with circles + 4 surrounding bubbles at
  placeholder-art quality (shapes and lines are fine; no bespoke art).
  Pan/zoom is P2 — a first version may fit the Alpha-sized tree on one
  screen.

### REQ-3: Skill effects — starter set (P0)
- Each bubble maps to a real, wired effect. Starter catalog kept small and
  built on existing tunables (examples, final list at implementer's
  discretion with operator sign-off in the PR: ability cooldown reduction,
  Controlled-duration bonus, energy max increase, pen-zone radius bonus,
  movement speed). Effects apply at run start via the existing
  pawn/component seams — no new parallel stat system.

### REQ-4: Modifier slots (P1) — ⚠️ partially implemented, issue #376
- 2 slots per unlocked skill; slotting UI in the P-Organ detail style (select
  skill → see its slots → pick from owned modifiers of the matching
  category/tier). Modifier acquisition source for Alpha: earned alongside
  mastery at level clear (simplest honest source; refine later).
- Tier gating: Tier II/III modifiers usable only when the tree has reached
  the corresponding phase.

  Issue #376 shipped the modifier catalog DataTable
  (`EModifierCategory`/`EModifierTier`/`FMasteryModifierRow`) and the
  `UCrowdMasteryTotalSubsystem` grant/slot/unslot API
  (`GrantModifier`/`TrySlotModifier`/`UnslotModifier`/`GetSlottedModifiers`),
  including the anti-duplication category rule and respec interaction, as
  session-only state with unit tests. Still open: modifier acquisition
  wiring (nothing calls `GrantModifier` yet), the slotting UI, and tier-gate
  enforcement (see `app-changelog/issue-376.md`).

### REQ-5: Respec integration (P0)
- The existing RESET → CONFIRM flow becomes full respec: zero the earned
  total only if it already did (unchanged semantics for the total), clear all
  unlocks and slotted modifiers, refund spent points before any zeroing —
  order and semantics unit-pinned. The menu display and tree screen both
  refresh immediately (the #349 lesson).

## Out of scope

- Steam sync (still future; storage already funnels through one authority).
- Balancing the skill effects beyond named starter values.
- Bespoke art, animation, controller navigation (P2 later).

## Existing surfaces to build on (do not reinvent)

- `UCrowdMasteryTotalSubsystem` (+ save-file persistence, #354) as the ONLY
  state owner; menu chrome + display + reset row (#332/#328/#329).
- C++-built widget lineage (`UMainMenuWidget` et al.); DataTable pattern
  (`DT_LevelSequenceTable`).
- The reference material: the two operator-supplied P-Organ screenshots and
  https://liesofp.wiki.fextralife.com/P-Organ for the slot/category/tier
  naming shape.
