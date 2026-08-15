---
description: Create a draft PR for the Dark Factory fix workflow, filling in the repo's PR template from implementation artifacts.
argument-hint: (reads workflow artifacts and $classify-issue.output)
---

Create a draft pull request for the current branch.

## Context

- **Issue**: $ARGUMENTS
- **Classification**: $classify-issue.output
- **Issue title**: $classify-issue.output.title

## Instructions

1. Check git status — ensure all changes are committed. If uncommitted changes exist, stage and commit them with a conventional-commits prefix (`feat:`, `fix:`, `chore:`, etc. per CLAUDE.md §Commit and PR Conventions, once that section exists).

   **app/-only changes (no tracked-repo diff) — mirror the real source into a tracked copy, don't just describe it.** `app/` is a gitignored symlink to the Windows-hosted Unreal project (CLAUDE.md's Environment section, `.factory/decisions.md` D-003) — most real gameplay work lands entirely there with zero files changed in this git repo. Check `git status --porcelain` and `git diff main...HEAD --stat` (or the equivalent against `$BASE_BRANCH`) **before** deciding there's nothing to commit. If both are empty:
   - Read `$ARTIFACTS_DIR/implementation.md` and `$ARTIFACTS_DIR/validation.md` (already required reading per step 3 below — do this check first). `implementation.md`'s "Files Changed" table lists exactly which files to mirror.
   - **Copy the actual file contents** (not a description of them) for every CREATE/UPDATE entry in that table from `app/<relative-path>` to `app-source-tracked/<same-relative-path>` in this repo, preserving the directory structure (e.g. `app/Source/KrowdKontrol/RoomEnemyBudgetController.h` → `app-source-tracked/Source/KrowdKontrol/RoomEnemyBudgetController.h`). This is a plain file copy of small text source files (`.h`/`.cpp`/`.Build.cs`) — never `.uasset`/`.umap`/anything under `Content/`/`Binaries/`/`Intermediate/`, so it carries none of the binary-asset/LFS/size concerns that keep the rest of `app/` untracked. `app/` itself stays exactly as-is (still a symlink, still gitignored, still what the Editor and the harness actually build against) — this is a *copy* for review purposes, not a new live link, so it can't reintroduce the WSL/Editor cross-boundary failure D-003 hit.
   - **Also** write `app-changelog/issue-{N}.md` (determine the issue number from `$ARGUMENTS`, `$classify-issue.output`, or the branch name `archon/task-fix-issue-N`; create the `app-changelog/` directory if needed) as a short human-readable index: issue number/title, one-paragraph summary, an explicit checklist mapping each acceptance criterion to how it's satisfied, and the validation evidence from `validation.md`. This is context for the reviewer, not the evidence itself — the mirrored files under `app-source-tracked/` are the actual evidence now, so keep this file short rather than re-describing what's already in the diff.
   - `git add app-source-tracked/ app-changelog/issue-{N}.md && git commit -m "..."` — this becomes the tracked-repo diff for this PR, containing real, reviewable source. Use a real conventional-commits message describing the underlying `app/` change, not "add source mirror" — this is infrastructure for the PR/review to work, not what the commit is actually about.
   - This does **not** replace `implementation.md`/`validation.md` as the fuller record — those stay in `$ARTIFACTS_DIR/` as always.

2. Push the branch: `git push -u origin HEAD`
3. Read implementation artifacts from `$ARTIFACTS_DIR/` for context:
   - `$ARTIFACTS_DIR/investigation.md` or `$ARTIFACTS_DIR/plan.md`
   - `$ARTIFACTS_DIR/implementation.md`
   - `$ARTIFACTS_DIR/validation.md`
4. Check if a PR already exists for this branch: `gh pr list --head $(git branch --show-current)`
   - If PR exists, skip creation and capture its number.
5. Look for the project's PR template at `.github/pull_request_template.md`, `.github/PULL_REQUEST_TEMPLATE.md`, or `docs/PULL_REQUEST_TEMPLATE.md`. Read whichever one exists.
6. Create a DRAFT PR: `gh pr create --draft --base $BASE_BRANCH`
   - **Title**: use the conventional-commits prefix matching the first commit (`feat:`, `fix:`, `chore:`, etc.), imperative mood, under 72 chars.
   - **Body**: if a PR template was found, fill in **every section** with details from the artifacts. Don't skip sections or leave placeholders. If no template, write a body with summary, changes, validation evidence, and `Fixes #...`.
   - **Required**: the body MUST contain `Fixes #N` (or `Closes #N` / `Resolves #N`) on its own line — CLAUDE.md §Commit and PR Conventions enforces this so the validator can extract the linked issue.
   - **New dependencies**: if the implementation added any dependencies (to whatever manifest this repo's stack uses — see CLAUDE.md for the actual files once there's a stack), include a "Dependencies" section per FACTORY_RULES.md §2 explaining what the dependency does, why existing dependencies don't work, and evidence of active maintenance.
7. Add the `factory:needs-review` label so the orchestrator picks this PR up for validation:
   ```bash
   gh pr edit --add-label "factory:needs-review"
   ```
8. Capture PR identifiers:
   ```bash
   PR_NUMBER=$(gh pr view --json number -q '.number')
   echo "$PR_NUMBER" > "$ARTIFACTS_DIR/.pr-number"
   PR_URL=$(gh pr view --json url -q '.url')
   echo "$PR_URL" > "$ARTIFACTS_DIR/.pr-url"
   ```
