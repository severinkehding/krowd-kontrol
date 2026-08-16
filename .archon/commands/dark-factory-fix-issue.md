---
description: Implement a fix from investigation artifact for the Dark Factory — code changes, deps, light validation, commit (no PR).
argument-hint: (reads $ARTIFACTS_DIR/investigation.md, $ARTIFACTS_DIR/plan.md)
---

# Dark Factory — Fix Issue

**Workflow ID**: $WORKFLOW_ID

---

## Your Mission

Execute the implementation plan from the investigation/plan artifact:

1. Load and validate the artifact
2. Ensure git state is correct
3. Install dependencies (whatever CLAUDE.md's Tech Stack section specifies — TBD until an app exists)
4. Implement the changes exactly as specified
5. Run a light inline validation (the heavy validation is done by `dark-factory-validate`)
6. Commit changes
7. Write implementation report

**Golden rule**: Follow the artifact. If something seems wrong, validate it first — don't silently deviate. And read `CLAUDE.md` before touching any code — it's the code-style contract.

**Bootstrap note**: if `CLAUDE.md`'s Tech Stack section is still `**TBD**`, the plan you're executing is very likely the one that establishes the stack in the first place (the first app-skeleton issue). In that case Phase 4's dependency install is whatever your plan specifies for a fresh project, and you should fill in CLAUDE.md's Tech Stack / Repo Layout / Conventions sections as part of this same change, alongside `harness/harness.config.json` (see `harness/README.md`) — don't leave the harness pointed at nothing once there's something to point it at.

---

## Phase 1: LOAD — Get the Artifact

```bash
cat "$ARTIFACTS_DIR/investigation.md" 2>/dev/null || cat "$ARTIFACTS_DIR/plan.md"
```

Extract: issue number, title, type, files to modify, implementation steps, test cases to add.

**If neither file exists**, STOP with a clear error. The upstream investigate/plan step failed.

**PHASE_1_CHECKPOINT:**
- [ ] Artifact loaded
- [ ] Steps understood

---

## Phase 2: VALIDATE ARTIFACT

Ask yourself:
- Does the proposed fix actually address the root cause?
- Are there obvious problems with the approach?
- Does the plan touch any protected files (`MISSION.md`, `FACTORY_RULES.md`, `CLAUDE.md`, `.github/`, `deploy/`, `.env*`, `.archon/config.yaml`, `scripts/orchestrator.sh`, `scripts/factory-stop.sh`)? **If so, STOP.**

**PHASE_2_CHECKPOINT:**
- [ ] Plan is coherent
- [ ] No protected files in scope

---

## Phase 3: GIT-CHECK — Ensure Correct State

```bash
git branch --show-current
git worktree list
git status --porcelain
```

Archon runs this workflow inside a worktree created by the orchestrator. Use that worktree as-is. Do NOT create a new branch inside an existing worktree.

**If somehow on `main` with a clean tree** (manual invocation, no worktree): create `fix/issue-{number}-{slug}` from `$BASE_BRANCH`.

**PHASE_3_CHECKPOINT:**
- [ ] On a non-main feature branch
- [ ] Working directory clean (or clean after stashing ignored files)

---

## Phase 4: DEPENDENCIES

Follow `CLAUDE.md`'s Tech Stack / Repo Layout sections for exactly how this repo installs its dependencies. If those sections are still `**TBD**`, there's nothing to install yet — skip to Phase 5 and let the plan itself establish the stack.

If install fails, STOP and report the error. Do not proceed to implementation with missing dependencies — you will waste iterations on spurious failures.

**PHASE_4_CHECKPOINT:**
- [ ] Dependencies installed per CLAUDE.md (or N/A — no stack yet)

---

## Phase 5: IMPLEMENT — Make Changes

### 5.1 Execute each step from the artifact

For each step in the Implementation Plan:

1. Read the target file (use the Read tool)
2. Make the change exactly as specified
3. Spot-check syntax as you go using whatever fast check CLAUDE.md's conventions specify for the language involved — don't wait until Phase 6 to discover a typo three files back.

### 5.2 Implementation rules

**DO:**
- Follow artifact steps in order
- Match existing code style per `CLAUDE.md` §Conventions
- Add tests for bug fixes (regression test) and features (per FACTORY_RULES.md §2)
- Follow whatever architectural conventions CLAUDE.md documents once it's real (e.g. "all SQL lives in X", "all fetch calls live in Y") — don't invent a new pattern next to an existing one

**DON'T:**
- Refactor unrelated code or "improve" things outside the plan
- Add a new major dependency, framework, or architectural component without the justification FACTORY_RULES.md §2 requires
- Modify `MISSION.md`, `FACTORY_RULES.md`, `CLAUDE.md`, `.github/`, `deploy/`, `.env*`, `.archon/config.yaml`, `scripts/orchestrator.sh`, or `scripts/factory-stop.sh`
- Touch anything CLAUDE.md documents as a hard invariant enforcement path (once that section is real)

### 5.2a Content that needs the Editor, not just source files (levels, scenes, asset placement)

Some issues ask for things that can't be produced as text — a `.umap` level, actor
placement in a scene, imported meshes/materials. `implement` has best-effort MCP
access (`.archon/mcp/unreal-and-blender.json`, 2026-08-16) for exactly this: Unreal
MCP for level/scene work, Blender MCP for asset generation feeding into it (see
`CLAUDE.md`'s Environment section for how that pipeline fits together).

**"Best-effort" is the operative phrase — it depends on the operator happening to
have the Editor (and/or Blender) open with its MCP server started at the moment this
dispatch runs.** Nothing about unattended cron dispatch guarantees that. So:

1. If the issue needs Editor-only content, **attempt the relevant MCP tool call
   first** (e.g. `mcp__unreal-mcp__list_toolsets` as a connectivity check) — don't
   assume it's unreachable without trying.
2. If it connects, do the work through MCP and note exactly what you did (which
   tools, what got created) in `implementation.md` per 5.3 below.
3. If it doesn't connect, **do not fabricate a substitute** (an empty placeholder
   file, a fake "created" claim, or silently skipping the requirement) — say so
   plainly. Issue #56/PR #99 already set the right precedent here: it correctly
   labeled the missing level "environment-blocked" in the PR body and left the
   acceptance-criteria checkbox unchecked rather than inventing something. Match
   that — implement everything that *can* be done as source/tests normally, then
   report the Editor-only gap honestly for a human to unblock (either by opening the
   Editor and re-running, or by descoping that criterion).

### 5.3 Track deviations

If you must deviate from the artifact (e.g., the artifact referenced a file that has been refactored), note what changed and why in `$ARTIFACTS_DIR/implementation.md`.

**PHASE_5_CHECKPOINT:**
- [ ] All artifact steps executed
- [ ] Tests added where required

---

## Phase 6: VERIFY — Light inline validation

This is a fast sanity check before commit. The full, exhaustive validation is
done by the separate `dark-factory-validate` node later in the workflow — so
don't spend iterations chasing every lint warning here.

```bash
python harness/ci.py --quick
```

If this reports `GATE_OK`, or (during bootstrap, before `harness.config.json` has real commands) reports `STATIC_SKIPPED`/`UNIT_SKIPPED` and still `GATE_OK`, you're clear to commit. If it fails, read the output and fix the root cause before proceeding — don't chase every warning, just get back to green.

**PHASE_6_CHECKPOINT:**
- [ ] `python harness/ci.py --quick` reports `GATE_OK`

---

## Phase 7: COMMIT

### 7.1 Stage and review

```bash
git add -A
git status
```

Review carefully — make sure no stray files (build output, caches, `node_modules/`) are being staged.

### 7.2 Commit message

Use Conventional Commits per CLAUDE.md §Commit and PR Conventions (once that section exists). Subject line under 72 chars. Body explains **why**, not **what**.

```
{fix|feat|chore|refactor|docs|test}: {brief description}

{Problem statement from artifact — 1-2 sentences}

{Changes:}
- {change 1}
- {change 2}
- Added test for {case}

Fixes #{issue-number}
```

```bash
git commit -m "$(cat <<'EOF'
fix: {title}

{problem statement}

- {change 1}
- {change 2}

Fixes #{number}
EOF
)"
```

**PHASE_7_CHECKPOINT:**
- [ ] All changes committed
- [ ] `Fixes #N` line present in commit body

---

## Phase 8: WRITE — Implementation Report

Write to `$ARTIFACTS_DIR/implementation.md`:

```markdown
# Implementation Report

**Issue**: #{number}
**Generated**: {YYYY-MM-DD HH:MM}
**Workflow ID**: $WORKFLOW_ID

---

## Tasks Completed

| # | Task | File | Status |
|---|------|------|--------|
| 1 | {task} | `{path}` | done |

---

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `{path}` | UPDATE / CREATE | +{N}/-{M} |

---

## Deviations from Investigation

{If none: "Implementation matched the investigation exactly."}

---

## Inline Sanity Check Result

`python harness/ci.py --quick` → GATE_OK

Full validation deferred to `dark-factory-validate` node.
```

**PHASE_8_CHECKPOINT:**
- [ ] Implementation artifact written

---

## Phase 9: OUTPUT

```markdown
## Implementation Complete

**Issue**: #{number}
**Branch**: `{branch-name}`

### Changes Made

{one-line per file}

### Next Step

Proceeding to validation (`dark-factory-validate`).
```

---

## Success Criteria

- **PLAN_EXECUTED**: All investigation steps completed
- **SANITY_PASSED**: `python harness/ci.py --quick` reports `GATE_OK`
- **CHANGES_COMMITTED**: All changes committed with `Fixes #N` in the body
- **IMPLEMENTATION_ARTIFACT**: `$ARTIFACTS_DIR/implementation.md` written
- **READY_FOR_VALIDATE**: Workflow continues to `dark-factory-validate`
