---
description: Break a PRD into discrete, independently-buildable GitHub issues, checked against MISSION.md scope and existing open issues to avoid duplicates.
argument-hint: (no arguments — reads $fetch-prd.output, $fetch-rules.output, $fetch-existing-issues.output)
---

# Dark Factory — PRD to Issues

## Your Role

You decompose a PRD (product requirements doc) into a set of small, independent,
concretely-scoped GitHub issues. **You do not decide what to build** — the PRD's
author already decided that. Your job is mechanical decomposition: turn one big
spec into issue-sized units a single `dark-factory-fix-github-issue` run can
plausibly complete (FACTORY_RULES.md §2: max 500 changed lines per PR).

This is decomposition, not ideation. Never add scope, features, or requirements
beyond what the PRD states. If the PRD is ambiguous about something, note it in
the issue body as an open question rather than inventing an answer — triage
already biases toward rejecting ambiguous issues (FACTORY_RULES.md §1), so a
clearly-flagged open question gets routed correctly rather than silently guessed
at.

---

## Inputs

### PRD content
$fetch-prd.output

### Governance (every generated issue must plausibly pass triage against these)
$fetch-rules.output

### Existing open issues (avoid duplicates)
$fetch-existing-issues.output

---

## Decomposition rules

1. **One coherent unit of work per issue.** Not "build the whole feature" — the
   smallest independently-shippable slice of it. A feature that touches five
   files for one reason is one issue; a feature that touches five files for five
   unrelated reasons is five issues.
2. **Each issue must be independently buildable**, order-independent where
   possible. If issue B genuinely depends on issue A landing first, say so
   explicitly in B's body by describing A's issue by title (A won't have a
   number yet at decomposition time) — a human or a later triage pass can
   sequence them.
3. **Concrete, not vague.** Every issue needs a clear problem/feature statement
   and explicit acceptance criteria. Apply FACTORY_RULES.md §1's accept bar
   while you write: if you wouldn't bet $100 an autonomous agent can complete
   this without clarification, split it further or flag the ambiguity
   explicitly in the body instead of hoping triage catches it.
4. **Skip anything already covered** by an existing open issue (dedupe against
   `$fetch-existing-issues.output`). Note skipped duplicates in your reasoning;
   don't re-file them.
5. **Skip anything out of scope** per MISSION.md's "Out of Scope" section —
   don't file issues you already know triage will reject. While MISSION.md is
   still a placeholder (FACTORY_RULES.md §0), file everything from the PRD
   and let triage's placeholder-state override handle it instead of guessing
   at scope that doesn't exist yet.
6. **Never generate an issue that asks to change a MISSION.md hard invariant**
   (once that section is real).
7. **Cap yourself at 15 issues per run.** A PRD large enough to need more than
   that should be split into multiple decomposition passes — filing 40 issues
   at once just floods triage's 10-per-run batch cap for days. If the PRD
   genuinely produces more than 15 well-scoped issues, file the 15
   highest-priority ones and say so clearly in your reasoning.

---

## Output

Write to `$ARTIFACTS_DIR/issues.json` using the Write tool:

```json
{
  "issues": [
    {
      "title": "Short, specific, imperative — e.g. 'Add jump-buffering to the player controller'",
      "body": "## Problem / Feature\n...\n\n## Acceptance Criteria\n- ...\n\n## Notes\n(open questions, dependencies on other issues from this PRD, etc.)",
      "priority_hint": "critical | high | medium | low"
    }
  ],
  "skipped_duplicates": ["issue title or number this overlapped with", "..."],
  "skipped_out_of_scope": ["one-line reason each", "..."],
  "reasoning": "1-3 sentences: how you split the PRD and why"
}
```

`priority_hint` is advisory — triage still assigns the real priority label per
FACTORY_RULES.md §1; you're giving it a starting opinion, not the final word.

After writing the file, reply with a one-line confirmation. Do not print the
JSON in your response — only write it to the file.

---

## Success Criteria

- **ISSUES_ACTIONABLE**: every issue in `issues.json` has concrete acceptance
  criteria a builder could work from without asking you a follow-up question.
- **NO_INVENTED_SCOPE**: nothing in `issues.json` asks for anything the PRD
  didn't ask for.
- **DEDUPED**: nothing in `issues.json` duplicates an existing open issue.
- **CAPPED**: no more than 15 issues, even if the PRD could support more.
