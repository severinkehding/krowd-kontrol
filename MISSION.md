# Mission

> 🚧 **PLACEHOLDER.** Nothing below this banner is real product scope yet. Until this
> file is filled in for real, `FACTORY_RULES.md` instructs triage to route every issue
> to `factory:needs-human` rather than guess at scope that doesn't exist. Filling this
> in is the natural next step after the harness itself is verified - not something the
> factory can do for itself, since choosing what krowd-kontrol is is a judgement value,
> not a product value (see `.factory/decisions.md`).

**Derived from:** *(no PRD yet)*
**Last reconciled with that PRD:** *(n/a)*

> This file is meant to be the PRD compressed to the part the factory has to obey. When
> the product changes, both files change in the same commit - otherwise the factory
> keeps faithfully building the old scope and nothing warns you.

## What krowd-kontrol Is

**TBD.**

## Who It's For

**TBD.**

## Core Capabilities (In Scope)

**TBD.**

## Out of Scope (Factory Must Never Build)

**TBD.** Until this section is real, treat everything as out of scope - triage should
defer to a human rather than guess.

## Hard Invariants (Not Tunable by Factory Issues)

**TBD.** These will be constraints the factory is explicitly forbidden from modifying
even if an issue asks nicely, explains a good reason, or claims it's a bug. At minimum,
once this section is real, it must restate:

1. **The factory cannot modify governance files.** `MISSION.md`, `FACTORY_RULES.md`,
   and `CLAUDE.md` are the constitution. Any PR that touches them is an automatic
   reject. (This one is not actually TBD - it holds even in the placeholder state.)

## Quality Standards (Definition of Done)

**TBD**, but whatever lands here must clear the same three gates
`FACTORY_RULES.md` §3 already enforces structurally: static checks passing, a UI (if
any) usable without external docs, and a real end-to-end regression via
`agent-browser` for anything that touches runnable code.

## Non-Goals

**TBD.**
