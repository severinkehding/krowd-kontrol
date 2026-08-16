# Mission

**Derived from:** the Krowd Kontrol PRD set (`/home/severin/krowd-kontrol-prds/`, 16
PRDs + `00-README.md`), itself a decomposition of the original Krowd Kontrol Game
Design Document (`/home/severin/krowd-kontrol-prds/og-google-doc/krowd-kontrol-gdd.md`).
**Last reconciled with that PRD:** 2026-08-15

> This file is meant to be the PRD compressed to the part the factory has to obey. When
> the product changes, both files change in the same commit - otherwise the factory
> keeps faithfully building the old scope and nothing warns you.

## What krowd-kontrol Is

Krowd Kontrol is a 2D top-down, **non-lethal** crowd-control action game, built in
Unreal Engine. The player character (Krowd) never kills anything — they herd, disable,
and redirect hostile robots (former friends, corrupted by the antagonist AI "Drain")
toward "target zones," where they're pacified rather than destroyed. The core fantasy
is *crowd management*, not combat: matching one of five colour-coded crowd-control
abilities (Stun / Sleep / Root / Fear / Snare) against one of four colour-coded enemy
types, under escalating crowd density (up to 20+ simultaneous enemies).

This is a **solo, ~240-hour, six-month practice/portfolio project** (not a studio
production; no monetization for v1), inspired by *From Space* and *Neon Chrome*. Visual
identity is "neon-noir": desaturated dark environments with five reserved
information-carrying neon colours. Release target is a free Itch.io listing.

The PRD set's own dates (originally April–October 2023) are stale — treat the
*durations* (one month per milestone, per `16-scope-milestones-and-success-metrics.md`)
as sound and re-anchor the absolute dates against the actual project start whenever
scheduling work begins; that re-anchoring is a human decision, not something a factory
issue should do on its own.

## Who It's For

- **Primary:** the solo developer/player themselves, and 5–20 informal alpha/beta
  playtesters recruited directly (per the PRD set's own success metrics) — this is a
  practice project first, a public release second.
- **Secondary, if it lands well:** a small Itch.io audience discovering the game
  post-release. Steam is an explicitly deferred, conditional stretch goal (see Out of
  Scope), not a target audience to build for now.

## Core Capabilities (In Scope)

Organized by the PRD set's own P0 (Alpha) / P1 (Open Beta) / P2 (post-launch stretch)
tiers — see `16-scope-milestones-and-success-metrics.md` for the full milestone
mapping. A factory issue's priority label should track which tier its requirement
falls in, not be assigned independently of it.

**P0 — required for the Alpha ("Playable Alpha" demo):**
- The core loop: perceive → prioritize → control (cast a CC ability) → herd (spatially
  guide a controlled enemy to a target zone) → bank → escalate → clear (`01`).
- All 5 crowd-control abilities (Stun, Sleep, Root, Fear, Snare), colour-coded,
  unlocked one per level, each viable solo at base effectiveness with a colour-matched
  bonus against its countered enemy type (`02`).
- All 4 core enemy types (RU-NNR, TR-UPR, B0-0MR, SN-1PR), each with a distinct
  silhouette and attack tell independent of colour, simple
  Idle→Alert→Attack→Controlled→Banked state-machine AI (`03`).
- 5 hand-authored levels with a difficulty ramp via room count and enemy density
  (`05`) — 5, not 3, so the one-ability-unlock-per-level sequence (`02` above) maps
  cleanly onto the level count (operator decision 2026-08-17, resolving the
  discrepancy issue #69 flagged).
- Diegetic onboarding: the entire game taught through forced-safe first encounters in
  the Opening Scene, no paused tutorial cards (`09`).
- Core HUD: energy meter, ability/cooldown tray, world-space target-zone beacons, none
  of it using the five reserved gameplay-information colours (`13`).
- 2D pipeline locked (Paper2D or flat-camera 3D) in Unreal Engine, placeholder-art-first
  asset pipeline, MCP-assisted iteration tooling (`14`).
- Neon-noir visual direction with the five-colour information lock enforced (`11`).
- Opening Scene narrative beat, interactive from frame one (`07`).

**P1 — required for Open Beta:**
- 3 mid-bosses + Drain (final boss) = 4 total boss encounters, each forcing use of a
  specific neglected ability, re-enterable without a full level restart (`04`).
- Room-pool + procedural connector level system (~15–20 tagged rooms, ability-gated
  shuffling) replacing the hand-authored Alpha levels (`05`).
- Replayability systems: per-level clear-time tracking, "Crowd Mastery" stat (largest
  simultaneous crowd herded), New Game+ "Overclock Mode" (`06`).
- Full narrative arc delivered via short Gizmo remote-call barks triggered by
  progression milestones, optional environmental storytelling for the Drain reveal
  (`07`).
- "Overcrowd" punishment (Punishment 3): screen distortion + increased drain rate when
  too many hot enemies converge uncontrolled — alongside the existing ability-lock
  (Punishment 1) and run-speed-reduction (Punishment 2) mechanics, with only one
  punishment state active at a time (`08`).
- Elite enemy variants (recoloured/stat-tweaked reskins of the 4 core types) from level
  4–5 onward (`03`).
- Locked 80s-electronic driving-bass music with a 2-state (calm/combat) adaptive
  system, per-enemy attack audio tells, distinct Overcrowd audio treatment, boss-fight
  music intensity swap (`12`).
- Post-run summary screen; colourblind-safe redundancy (shape/icon, not colour alone)
  for the 5-colour ability/enemy system (`13`).
- Itch.io release: free, no IAP/ads, storefront-readiness checklist (footage,
  description, control-scheme summary), external feedback form for playtests — no
  in-game telemetry infra (`15`).

**P2 — post-launch / stretch, only after all P0/P1 above ships and only if
playtesting shows appetite:**
- Local (couch) co-op, 2 players, same-screen, independent per-player energy with a
  downed/revived state — reusing single-player systems unchanged. Attempted only after
  every other P0/P1 item across this list is complete (`10`).
- Activation-delay ability variants; a 4th optional/secret mid-boss (`02`, `04`).
- Daily/weekly seeded runs, compared informally, not via an in-game leaderboard (`06`).
- Steam release — gated behind "overwhelmingly positive" Itch.io feedback, treated as
  an entirely separate future scoping exercise, not pre-planned now (`15`).

## Out of Scope (Factory Must Never Build)

- **Online multiplayer, matchmaking, session keys, or any networked backend service**,
  at any point, regardless of how an issue frames it — explicitly and indefinitely out
  of scope, not just deferred. A solo 240-hour budget cannot absorb real-time networked
  multiplayer; even "add basic multiplayer" issues must be rejected. (Local/couch co-op
  is the only multiplayer in scope, and only as a P2 stretch — see above.)
- **Any monetization system** — in-app purchases, ads, DLC storefront infrastructure,
  paywalls. The v1 release is free on Itch.io by design.
- **Steam-specific integration work** (achievements, trading cards, Steamworks SDK)
  before the Itch.io release has actually met its own "overwhelmingly positive
  feedback" bar. Do not pre-build this speculatively.
- **A skill-tree / ability-upgrade system**, or any new ability beyond the 5 already
  locked (Stun, Sleep, Root, Fear, Snare). The roster is intentionally fixed.
- **Any 5th (or additional) core enemy type.** The 4-type roster (RU-NNR, TR-UPR,
  B0-0MR, SN-1PR) plus elite reskins is deliberately locked — depth comes from density
  and mixing, not roster growth.
- **Branching dialogue trees or player-choice/morality systems.** Narrative is
  delivered as linear one-sided Gizmo remote-call barks, by design.
- **A traditional, separate tutorial level.** Onboarding is folded into the real
  Opening Scene — a distinct "tutorial room" contradicts the locked design.
- **Full voice acting as a blocking requirement.** It's a P2 stretch, never something a
  build should be gated on.
- **A full roguelike run-structure overhaul** — permadeath, randomized ability-unlock
  order, or similar large structural changes to the run — explicitly flagged as too
  large a change for this project's scope.
- **Live-service content cadence** — seasons, ongoing scheduled post-launch content
  commitments. This is a scoped, finite release, not a service.
- **Full procedural room *geometry* generation.** Only room-*pool* shuffling
  (pre-authored rooms, procedurally sequenced) is in scope — algorithmic room-layout
  generation is explicitly the larger, riskier path the PRD set rejected.
- **In-game telemetry or analytics infrastructure**, and **in-game feedback/leaderboard
  systems.** An external form (Google Form or similar) is the entire feedback
  mechanism for this project's scope.
- **Distributed build/CI infrastructure.** This is solo, local-only development — a
  cloud build pipeline is explicitly not a goal here.

## Hard Invariants (Not Tunable by Factory Issues)

These hold regardless of what an issue asks for, how it's framed, or what rationale it
offers:

1. **The factory cannot modify governance files.** `MISSION.md`, `FACTORY_RULES.md`,
   and `CLAUDE.md` are the constitution. Any PR that touches them is an automatic
   reject.
2. **No enemy — normal, elite, or boss — is ever killed.** The player herds, disables,
   and redirects; defeated enemies are "banked" (pacified) at a target zone, never
   destroyed. This is the game's entire identity. The one narrow, already-established
   exception: Drain is a disembodied AI, not a robot body, and is "deleted," not
   killed — this exception does not extend to any enemy with a physical body.
3. **The five gameplay-information colours are a locked, hard-reserved channel.**
   Purple (RU-NNR / Snare), Teal (TR-UPR / Root), Orange (B0-0MR / Fear), Blue (SN-1PR
   / Sleep), White (player / Stun). No other gameplay-relevant object, UI chrome, or
   environmental prop may use these five colours for non-informational purposes. A 6th
   saturated information colour must never be introduced.
4. **The ability roster is exactly 5: Stun, Sleep, Root, Fear, Snare.** Stun is
   deliberately colour-neutral with no enemy counter — do not add one. No 6th ability.
5. **The core enemy roster is exactly 4 types: RU-NNR, TR-UPR, B0-0MR, SN-1PR**, plus
   elite reskins of those same 4, plus the 3 mid-bosses and Drain. No net-new enemy
   types.
6. **Engine and dimensionality are locked: Unreal Engine, 2D** (Paper2D or
   flat-camera 3D). This is a foundational decision every other system depends on — do
   not revisit it via a factory issue.
7. **Online multiplayer/networking infrastructure must never be built** — restated
   from Out of Scope because it is the single highest-risk scope-creep vector this
   project has.
8. **The Unreal project itself is not tracked in this git repository** and lives
   outside it (see `CLAUDE.md`'s Environment section for its path and why). See
   `FACTORY_RULES.md` §8 for the operational consequence (serialized dispatch) this
   requires until that changes. **Narrow exception (2026-08-15, D-009):**
   `app-source-tracked/` is a plain-text *copy* of specific new/changed `.h`/`.cpp`/
   `.Build.cs` source files, written by `create-pr` at PR-creation time so GitHub can
   open a PR and reviewers can see real code — this is not the project (no
   `.uasset`/`.umap`/`Content/`/`Binaries/`/`Intermediate/`, no live link, `app/`
   itself stays exactly as untracked as before). A PR containing only
   `app-source-tracked/` + `app-changelog/` entries matching its own linked issue is
   compliant with this invariant, not a violation of it — see `CLAUDE.md`'s Environment
   section for the full rationale before flagging one.

## Quality Standards (Definition of Done)

Whatever lands here must clear the gates `FACTORY_RULES.md` §3 enforces structurally:
static checks passing (once any exist for this stack) and a real behavioral
regression. For this project that regression is an **Unreal MCP-driven visual QA
pass** (the `unreal-agent-harness` skill's `ue_qa.py` viewport-capture/decode/refdiff
workflow, or the built-in Unreal Automation Testing Framework run headlessly once real
automated tests exist) — **not** a browser-based check (`agent-browser`); this project
has no browser or web UI. See `FACTORY_RULES.md` §4 and `.factory/decisions.md` D-004
for the current, honestly-unfinished state of that gate.

Beyond the mechanical gate, "done" for any krowd-kontrol change means:
- It doesn't violate the no-kill rule, the 5-colour lock, or any other Hard Invariant
  above.
- It matches the PRD set's own P0/P1/P2 tiering — a P2 idea should never get built
  ahead of unfinished P0/P1 work in the same system.
- Placeholder-first: new gameplay elements are built and functional with
  primitive/placeholder art before any marketplace (Fab.com/Unreal Store) asset
  sourcing happens for them.

## Non-Goals

- Commercial/studio-scale production values. This project's budget is fixed at ~240
  hours; every requirement is written to fit inside that, not to be "as good as
  possible" without a budget ceiling.
- Building for a large or unknown audience. The target is 5–20 named playtesters per
  milestone, then a modest, free Itch.io audience — not growth, virality, or
  live-service retention.
- Solving "why would someone play this forever." The replayability systems in scope
  (P1) are explicitly the cheap, small-scope answer to that question, not an attempt
  to build a genuinely infinite-replay roguelike.
