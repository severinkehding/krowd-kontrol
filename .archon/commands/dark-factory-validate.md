---
description: Run the Dark Factory validation gate (harness/ci.py) and fix any failures.
argument-hint: (no arguments — reads $ARTIFACTS_DIR/implementation.md and git diff)
---

# Dark Factory Validation

**Workflow ID**: $WORKFLOW_ID

---

## Your Mission

Run this repo's validation gate and fix any failures before handing off to `create-pr`. The gate is a single entrypoint — `python harness/ci.py` — so this command stays the same across every stack this repo ever uses; only `harness/harness.config.json` changes (see `harness/README.md`).

**Golden rule**: run the gate, read the error, fix the root cause (not the test), re-run until green. Never modify tests to make them pass — that's an explicit CLAUDE.md/FACTORY_RULES.md violation.

---

## Phase 1: SCOPE — What Did the Implementation Touch?

```bash
git diff --name-only $BASE_BRANCH...HEAD
```

**Hard rules (FACTORY_RULES.md §5 — Protected Files):**

- If the diff touches `FACTORY_RULES.md`, `MISSION.md`, or `CLAUDE.md` — STOP and
  write a validation BLOCKED artifact. Governance files are not factory-editable.
- If the diff touches `.github/`, `deploy/`, Dockerfiles, `.env*`, `.archon/config.yaml`,
  `scripts/orchestrator.sh`, or `scripts/factory-stop.sh` — STOP and write a validation
  BLOCKED artifact.
- If nothing outside `*.md` / `docs/` changed, run the docs-only fast path (§4 below).

**PHASE_1_CHECKPOINT:**
- [ ] Touched files identified
- [ ] No protected files modified

---

## Phase 2: RUN THE GATE

```bash
python harness/ci.py
```

(Full mode, not `--quick` — this is the pre-PR check, and the quick subset already ran inline during implementation. Full mode also drives the app + e2e floor + holdout + mutations, whichever `harness.config.json` has configured.)

**If it fails:**

1. Read the `GATE_FAILED: <step>` line — it names exactly which rung stopped the run.
2. Fix the root cause in source. If the failure is in a check command itself (e.g. a
   typo in `harness.config.json`), fix that instead.
3. Re-run. Repeat until `GATE_OK` (full mode) or you've confirmed the failure is
   pre-existing and unrelated to this diff (rare — note it in the artifact rather than
   "fixing" someone else's problem).

**Bootstrap case**: if `harness.config.json` has no `static`/`unit`/`http` configured yet (see `harness/README.md`), full mode will fail at the app-boot step (`AppDidNotStart`) — that's expected until an app exists, not a bug in your diff. In that case, run `python harness/ci.py --quick` instead, confirm it reports `GATE_OK mode=quick`, and note in the artifact that full-mode validation is not yet possible in this bootstrap state.

**Record result**: `GATE_OK` (full) / `GATE_OK` (quick, bootstrap case) / `GATE_FAILED: <step>` (fixed, then re-run to confirm)

**PHASE_2_CHECKPOINT:**
- [ ] Gate passes, or the bootstrap exception applies and is documented

---

## Phase 3: HARD INVARIANTS (once MISSION.md defines any — always check, cheap guard)

If MISSION.md's Hard Invariants section is real, and the diff touches any file CLAUDE.md
names as implementing one, verify each invariant still holds by inspection — not just
by the gate passing. A hard invariant can regress in a way static checks don't catch
(e.g. a cap value silently duplicated in two places, only one of which got updated).

Any regression here is an automatic validation FAIL — even if the gate passes. Write the
regression into the artifact and stop.

**PHASE_3_CHECKPOINT:**
- [ ] Hard invariants verified (or skipped — none defined yet / diff doesn't touch them)

---

## Phase 4: ARTIFACT — Write validation.md

```markdown
# Validation Results

**Generated**: {YYYY-MM-DD HH:MM}
**Workflow ID**: $WORKFLOW_ID
**Status**: {ALL_PASS | FIXED | BLOCKED | BOOTSTRAP_QUICK_ONLY}

---

## Summary

| Check                | Result                    |
|-----------------------|---------------------------|
| `harness/ci.py`       | GATE_OK / GATE_OK (quick) |
| Hard invariants       | pass / N/A                |

---

## Files Modified During Validation

{If validation had to fix any files, list them with a one-line reason per file.}

---

## Issues Remaining

{If BLOCKED: what check failed, what was tried, what manual intervention is needed.}
```

### 4.1 Docs-only fast path

If Phase 1 determined the diff is docs-only, skip Phases 2-3 entirely and write:

```markdown
# Validation Results

**Status**: ALL_PASS
**Skipped**: the harness gate (no source changes)

This PR is documentation-only. Reviewed that only `.md` files and/or `docs/` were
modified.
```

**PHASE_4_CHECKPOINT:**
- [ ] `$ARTIFACTS_DIR/validation.md` written
- [ ] Status accurately reflects what ran and what passed

---

## Phase 5: OUTPUT — Report back to the workflow

### If all pass:

```markdown
## Validation Complete

**Workflow ID**: `$WORKFLOW_ID`

harness/ci.py: GATE_OK

Artifact: `$ARTIFACTS_DIR/validation.md`

Next: proceed to create-pr.
```

### If blocked:

```markdown
## Validation BLOCKED

**Workflow ID**: `$WORKFLOW_ID`

### Failed check
{step-name}: {short error summary}

### What was tried
1. {attempt 1}
2. {attempt 2}

### Required action
{what needs manual intervention — or why this is a real bug in the implementation
that the implement step produced}

Artifact: `$ARTIFACTS_DIR/validation.md`
```

---

## Success Criteria

- **GATE_PASS**: `python harness/ci.py` reports `GATE_OK` (or the documented bootstrap
  exception applies)
- **HARD_INVARIANTS_PASS**: no regressions per MISSION.md's Hard Invariants (once real)
- **ARTIFACT_WRITTEN**: `$ARTIFACTS_DIR/validation.md` exists with accurate status
