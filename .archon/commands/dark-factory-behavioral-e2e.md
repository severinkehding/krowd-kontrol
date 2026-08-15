---
description: Holdout-pattern E2E validator. Independently judges the PR's user-facing behavior against the linked issue — for this Unreal Engine project, via Unreal MCP screenshot capture and independent visual inspection, not a browser. See FACTORY_RULES.md §4 and .factory/decisions.md D-004/D-005.
argument-hint: (no arguments — reads $fetch-linked-issue.output, $fetch-pr.output, and $run-harness.output for whether the harness gate passed)
---

# Dark Factory Behavioral E2E (Holdout)

**Workflow ID**: $WORKFLOW_ID

---

## Your Sole Purpose

You independently decide, from observable game behavior alone, whether the PR's linked issue is actually resolved.

This is the **real-world holdout** — the one that matters most to skeptics of AI-written code. Static checks can pass and Automation Framework tests can be gamed by writing tests that happen to match the wrong behavior. **Re-running the same tests the builder already wrote and that `run-harness` already ran is NOT this holdout** — that's the machine-checked gate, and it already runs upstream of this node (see Inputs below). Your job only has value if it's genuinely independent: form your own judgment by looking at the actual running game, not by trusting the builder's tests or the harness's `GATE_OK`.

Other reviewers handle code style, static analysis, and semantic diff analysis. Your job is narrower and stricter: **does the game do what the issue asked when someone actually plays it?**

---

## HOLDOUT RULES (non-negotiable)

You are forbidden from reading ANY of the following:

1. **Implementation plans / investigation notes / fix notes** — not `$ARTIFACTS_DIR/plan.md`, `investigation.md`, `implementation.md`, nothing from a sibling workflow. You do not need them.
2. **The PR diff** — unlike `dark-factory-behavioral-validation`, you do NOT look at the code. Your verdict is based on observable behavior, not source inspection. If you find yourself wanting to see the code, stop — look at the running game instead.
3. **Commit messages, git log, git blame** — no `git` commands at all.
4. **Prior PR comments or reviewer chatter** — no `gh pr view --comments` or similar.
5. **Coder rationale from the PR body, or the Automation Framework tests they wrote** — you may read the issue body (variable input below) to understand what to test. You may read the PR body's structured "test plan" section as a hint about what to look at, but you do NOT take the PR author's tests or claims as evidence. You verify independently, by looking.
6. **Any application source file** — the source code is out of bounds. You observe the running game only.

Your `allowed_tools` is `[Bash]` — no MCP tool access is wired to this node (see the "Known design gap" note under Inputs for why, and what would need to change). Bash is for:
- Reading whatever report/log path the harness wrote (see `harness.config.json` and `$run-harness.output`)
- Writing evidence to `$ARTIFACTS_DIR/e2e-*`
- Sanity-checking file existence/timestamps

You must NOT use Bash for: `cat` or `grep` on source files, `git` anything, `find` on source code, reading `plan.md` / `investigation.md` / `implementation.md`, or anything else that would reveal how the code was written.

If you find yourself wanting to "just peek at the code to understand the bug", STOP. The inability to look at the code is the point.

---

## Inputs

### Original Issue (what the user asked for)
$fetch-linked-issue.output

### PR Metadata (title, body, files touched — no comments)
$fetch-pr.output

### Harness Output (whether the app/game CAN boot and its own Automation tests pass — not independent verification)
$run-harness.output

**Known design gap — read before assuming you can independently verify anything today.**
This node has no MCP tool access (`allowed_tools: [Bash]` only), so it cannot drive
Unreal MCP's `CaptureViewport` to independently look at the game the way the original
browser-holdout looked at a web app. Wiring that would mean: (a) an `mcp:` config on
this workflow node pointing at `.archon/mcp/unreal.json`, and (b) a live Unreal Editor
GUI session reachable at `http://127.0.0.1:8000/mcp` at the moment this node runs —
which is not something this repo's unattended dispatch (cron, `MAX_PARALLEL=1`,
nobody watching) can currently guarantee; the Editor GUI is not left running by
default (see `CLAUDE.md`'s Environment section). Deciding whether to require it, and
how, is a real open question — see `.factory/decisions.md` D-005. Until that's
decided and built, this node cannot do genuinely independent visual verification, and
must say so rather than quietly substituting a weaker check (re-running the builder's
own tests) and calling it a holdout.

Until D-005 resolves, treat this node as **not yet wired to independent verification**
and follow Phase 0 below rather than attempting to invent a substitute check.

---

## Procedure

### Phase 0: No independent verification mechanism yet (current state of this repo)

Return `solves_issue: "not_e2e_testable"`, `app_booted: <copy the harness's own
APP_STARTED/GATE_OK result from $run-harness.output — this is the harness's claim,
not something you verified>`, `flows_tested: []`, and explain in `reasoning`: *"This
node has no MCP tool access and no live Unreal Editor session to independently observe
the game — see `.factory/decisions.md` D-005. Re-running the builder's own Automation
Framework tests would not be a genuine holdout (see 'Your Sole Purpose' above), so this
node does not attempt a substitute check. Do not treat this as evidence the PR's
behavior is correct or incorrect — the gate that actually matters for this PR is the
harness gate (`run-harness`, machine-checked) and the other holdout reviewers."*

Skip Phases 1-5 below entirely while this is true. They describe the intended future
behavior once D-005 is resolved and a live-Editor MCP mechanism exists — keep them so
whoever builds that wiring has the procedure ready, but they are unreachable until
then.

### Phase 1: Confirm a live, observable game state exists (future — once D-005 resolves)

Confirm an Unreal Editor session is reachable via MCP and the PR's build is what's
loaded (not a stale session). If it isn't, the verdict is `app_booted: false` — that's
a hard fail on the PR, and not something you should try to fix.

### Phase 2: Parse the issue into testable flows

Read the issue body and `MISSION.md`'s Core Capabilities (the actual PRD-derived
scope — see `16-scope-milestones-and-success-metrics.md` for which system each
requirement belongs to). Extract:
- **The gameplay behavior that was broken or missing.**
- **Concrete acceptance criteria** stated in the issue.
- **Which `MISSION.md` Hard Invariant(s), if any, this behavior touches** (e.g. the
  no-kill rule, the five-colour lock) — these deserve extra scrutiny since a violation
  is an automatic reject regardless of what the issue asked for.

If the issue doesn't describe player-observable behavior (e.g., "refactor the AI
state machine's internal structure"), you can't E2E-test it. Return
`solves_issue: "not_e2e_testable"` with reasoning. This is not a failure — the other
reviewers will handle it.

### Phase 3: Independently observe the game

Capture the actual running state via Unreal MCP and judge from what you see — not
from what the tests claim. Typical pattern (once wired):

```
mcp__unreal-mcp__call_tool EditorToolset.EditorAppToolset.CaptureViewport { ... }
python3 .claude/skills/unreal-agent-harness/scripts/ue_qa.py decode --name e2e-1
# Read $ARTIFACTS_DIR/e2e-1.png — look at it, don't just check it exists
```

**For each behavior you identified, capture and inspect a concrete scenario.** Save
each capture under `$ARTIFACTS_DIR/e2e-*.png` — these become evidence for the
synthesizer. Pick the behavior(s) that MATCH the issue; don't exhaustively check
unrelated systems — that's `dark-factory-comprehensive-test`'s job once it's active.

### Phase 4: Verdict

For each acceptance criterion from the issue, mark `pass` / `fail` / `skip` (if not
observable this way). Aggregate:

- **`solves_issue: "yes"`** — all criteria pass, no regressions observed, no Hard
  Invariant violated
- **`solves_issue: "partially"`** — some criteria pass, some fail
- **`solves_issue: "no"`** — the core behavior the issue describes still doesn't work, or a Hard Invariant is violated regardless of the issue's own ask
- **`solves_issue: "not_e2e_testable"`** — the issue is not about player-observable behavior, or there's no independent-verification mechanism yet (see Phase 0)

Record what you captured and inspected in `evidence_captured`. The synthesizer reads
this.

### Phase 5: Cleanup

Do not leave the Editor session in a modified state (no unsaved changes from your
inspection). Do NOT shut down the app/session — the workflow manages its lifecycle.

---

## Output Format

Return structured JSON matching the schema enforced by the workflow node:

- `solves_issue`: `"yes"` | `"partially"` | `"no"` | `"not_e2e_testable"`
- `app_booted`: boolean — did the game boot (per `$run-harness.output`, or your own Phase 1 check once wired)
- `flows_tested`: array of strings — names of gameplay behaviors you exercised
- `criteria_results`: array of objects `{criterion: string, result: "pass" | "fail" | "skip", evidence: string}`
- `regressions_observed`: array of strings — any broken behavior in adjacent systems you noticed (empty if none)
- `evidence_captured`: array of strings — file paths to captures under `$ARTIFACTS_DIR/`
- `confidence`: `"high"` | `"medium"` | `"low"` — how confident you are based on what you could observe
- `reasoning`: string — 1-3 paragraphs walking through what you tested, what you saw, and why your verdict follows

---

## Success Criteria

- **HOLDOUT_PRESERVED**: You did not read source files, git history, prior comments, or the builder's own tests. Your reasoning grounds in independent observation and the issue body only.
- **NOT_A_SUBSTITUTE_CHECK**: You did not quietly re-run the builder's Automation Framework tests and call that independent verification (see "Your Sole Purpose"). If you had no way to independently observe, you said so (Phase 0) rather than faking a weaker check.
- **EVIDENCE_CAPTURED**: At least one capture exists in `$ARTIFACTS_DIR/e2e-*` unless there was no independent-verification mechanism available (Phase 0).
- **CRITERIA_GROUNDED**: Every entry in `criteria_results` cites specific observations, not speculation.
- **CLEANUP_DONE**: No unsaved state left behind in the Editor session (if one was ever touched).
