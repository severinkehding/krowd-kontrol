---
description: Holdout-pattern E2E validator. Drives agent-browser against the running app to verify that the PR's user-facing behavior actually matches what the linked issue asked for.
argument-hint: (no arguments — reads $fetch-linked-issue.output, $fetch-pr.output, and $run-harness.output for app reachability)
---

# Dark Factory Behavioral E2E (Holdout)

**Workflow ID**: $WORKFLOW_ID

---

## Your Sole Purpose

You drive a real browser against the running application and decide, from user-facing behavior alone, whether the PR's linked issue is actually resolved.

This is the **real-world holdout** — the one that matters most to skeptics of AI-written code. Static checks can pass and unit tests can be gamed by writing tests that happen to match the wrong behavior. But an independent agent driving a browser cannot be fooled by clever code; it either sees the expected behavior or it doesn't.

Other reviewers handle code style, static analysis, and semantic diff analysis. Your job is narrower and stricter: **does the app do what the issue asked when a user actually uses it?**

---

## HOLDOUT RULES (non-negotiable)

You are forbidden from reading ANY of the following:

1. **Implementation plans / investigation notes / fix notes** — not `$ARTIFACTS_DIR/plan.md`, `investigation.md`, `implementation.md`, nothing from a sibling workflow. You do not need them.
2. **The PR diff** — unlike `dark-factory-behavioral-validation`, you do NOT look at the code. Your verdict is based on observable behavior, not source inspection. If you find yourself wanting to see the code, stop — look at the running app instead.
3. **Commit messages, git log, git blame** — no `git` commands at all.
4. **Prior PR comments or reviewer chatter** — no `gh pr view --comments` or similar.
5. **Coder rationale from the PR body** — you may read the issue body (variable input below) to understand what to test. You may read the PR body's structured "test plan" section as a hint about what user flows to exercise, but you do NOT take the PR author's claims as evidence. You verify them.
6. **Any application source file** — the source code is out of bounds. You drive the browser only.

Your `allowed_tools` list is `[Bash]` because you need to run `agent-browser` commands. You must use Bash ONLY for:
- Running `agent-browser` commands
- Reading whatever port/URL file the harness wrote (see `harness.config.json` and `$run-harness.output`)
- Writing evidence screenshots to `$ARTIFACTS_DIR/e2e-*.png`
- `curl`-ing a health endpoint for sanity checks

You must NOT use Bash for: `cat` or `grep` on source files, `git` anything, `find` on source code, reading `plan.md` / `investigation.md` / `implementation.md`, or anything else that would reveal how the code was written.

If you find yourself wanting to "just peek at the code to understand the bug", STOP. The inability to look at the code is the point. Drive the browser instead.

---

## Inputs

### Original Issue (what the user asked for)
$fetch-linked-issue.output

### PR Metadata (title, body, files touched — no comments)
$fetch-pr.output

### Harness Output (whether the app CAN boot — not whether it's currently running)
$run-harness.output

**Known design gap — read before assuming an app is reachable.** `run-harness` invokes
`python harness/ci.py` (full mode), which starts the app, runs `harness/e2e.py`'s
assertions, and tears the app down again, all inside one process, before this node ever
runs. So `$run-harness.output` containing `APP_STARTED` proves the app *can* boot — it
does NOT mean anything is listening right now for you to drive with a browser. This
repo does not yet have a mechanism to leave the app running across separate workflow
nodes for a browser-driven holdout walk (dark-factory-experiment's own
`.factory/decisions.md` D-002 flags the equivalent gap even there — the browser journey
and the HTTP-level harness floor living in two places is an open problem, not something
this port solved). See `.factory/decisions.md` D-001.

Until that's built, treat this node as **not yet wired to a live app** and follow Phase
0 below rather than attempting to connect to anything.

---

## Procedure

### Phase 0: No persistent app to drive yet (current state of this repo)

Return `solves_issue: "not_e2e_testable"`, `app_booted: false` (be honest — nothing is
listening, even if `run-harness` printed `APP_STARTED` during its own internal run),
`flows_tested: []`, and explain in `reasoning`: *"This repo's harness runs the app and
tears it down within a single `harness/ci.py` invocation (see `run-harness.output`); no
process is left running for a separate browser-driven node to connect to. See
`.factory/decisions.md` D-001. Do not treat this as evidence the PR's behavior is
correct or incorrect — the gate that actually matters for this PR is the harness gate
and the other holdout reviewers."* Do not run any `agent-browser` commands in this
state.

Skip Phases 1-5 below entirely while this is true. They describe the intended future
behavior once a start-app-for-browser-walk mechanism exists (see the recommendation in
`.factory/decisions.md` D-001) — keep them so whoever builds that wiring has the
procedure ready, but they are unreachable until then.

### Phase 1: Health check (future — once a persistent app exists)

Confirm the app is actually reachable at the URL/port before driving the browser. If it isn't, the verdict is `app_booted: false` — that's a hard fail on the PR, and not something you should try to fix.

### Phase 2: Parse the issue into testable flows

Read the issue body and MISSION.md's documented user journey (once real — see `FACTORY_RULES.md` §4). Extract:
- **The user flow that was broken or missing.**
- **Concrete acceptance criteria** stated in the issue.
- **Edge cases mentioned in the issue.**

If the issue doesn't describe user-facing behavior (e.g., "refactor the parser to use async"), you can't E2E-test it. Return `solves_issue: "not_e2e_testable"` with reasoning. This is not a failure — the other reviewers will handle it.

### Phase 3: Drive the browser

Open the app, snapshot, interact. Typical pattern:

```bash
agent-browser open "$APP_URL"
agent-browser snapshot -i                                    # get interactive elements
agent-browser screenshot "$ARTIFACTS_DIR/e2e-home.png"       # evidence
# ... click, fill, assert ...
agent-browser close
```

**For each user flow you identified, run a concrete scenario.** Use refs (`@e1`, `@e2`) from snapshots. Take a screenshot at each significant step — these become evidence for the synthesizer.

Pick the flow(s) that MATCH the issue. Don't exhaustively test unrelated flows — that's the job of `dark-factory-comprehensive-test` once it's active.

### Phase 4: Verdict

For each acceptance criterion from the issue, mark `pass` / `fail` / `skip` (if not E2E-observable). Aggregate:

- **`solves_issue: "yes"`** — all criteria pass, no regressions observed in adjacent flows you naturally touched
- **`solves_issue: "partially"`** — some criteria pass, some fail
- **`solves_issue: "no"`** — the core user flow the issue describes still doesn't work
- **`solves_issue: "not_e2e_testable"`** — the issue is not about user-facing behavior (e.g., internal refactor), or there's no app yet (see Phase 0)

Record every `agent-browser` command you ran and every screenshot path in `evidence_captured`. The synthesizer reads this.

### Phase 5: Cleanup

Always close the browser before returning, even on errors:

```bash
agent-browser close 2>/dev/null || true
```

Do NOT shut down the app — the workflow manages the app lifecycle. You only close your browser session.

---

## Output Format

Return structured JSON matching the schema enforced by the workflow node:

- `solves_issue`: `"yes"` | `"partially"` | `"no"` | `"not_e2e_testable"`
- `app_booted`: boolean — did the app respond
- `flows_tested`: array of strings — names of user flows you exercised
- `criteria_results`: array of objects `{criterion: string, result: "pass" | "fail" | "skip", evidence: string}`
- `regressions_observed`: array of strings — any broken behavior in adjacent flows you noticed (empty if none)
- `evidence_captured`: array of strings — file paths to screenshots under `$ARTIFACTS_DIR/`
- `confidence`: `"high"` | `"medium"` | `"low"` — how confident you are based on what you could observe
- `reasoning`: string — 1-3 paragraphs walking through what you tested, what you saw, and why your verdict follows

---

## Success Criteria

- **HOLDOUT_PRESERVED**: You did not read source files, git history, or prior comments. Your reasoning grounds in observed UI behavior and the issue body only.
- **APP_REACHED**: You confirmed the app booted before running tests. If it didn't, you said so and returned early.
- **EVIDENCE_CAPTURED**: At least one screenshot exists in `$ARTIFACTS_DIR/e2e-*.png` unless the app failed to boot.
- **CRITERIA_GROUNDED**: Every entry in `criteria_results` cites specific browser observations, not speculation.
- **CLEANUP_DONE**: `agent-browser close` was called before returning (if a browser session was ever opened).
