# Issue #58: Add placeholder-asset content folder convention and policy README

Adds `app/Content/_Placeholder/` with four gameplay-category subfolders
(`Characters/`, `Enemies/`, `Abilities/`, `TargetZones/`) and a `README.md`
codifying the placeholder-first Definition-of-Done from `MISSION.md` and the
marketplace-replacement sequencing from PRD 14 (REQ-2/REQ-3, as referenced in
issue #58). Pure content-folder scaffolding: no gameplay code, no `.uasset`/`.umap`
files, no C++ — five empty directories and one Markdown file, all under
`app/Content/`. This unblocks PRDs 01 (core loop), 02 (abilities), 03 (enemies),
and 05 (levels), which would otherwise each invent their own ad hoc placeholder
folder convention.

Unlike prior C++ issues, this change has zero `.h`/`.cpp`/`.Build.cs` footprint,
so the `app-source-tracked/` mirror mechanism (D-009) does not apply — there is no
source file to mirror. This changelog is the tracked-repo record of the change per
D-009's precedent for `app/`-only, non-source issues.

## Files changed (all under `app/`, gitignored per D-003 — see closing note)

| File | Action | Contents |
|------|--------|----------|
| `app/Content/_Placeholder/Characters/` | CREATE (directory) | Feeds PRD 01 (core loop) — player character placeholder art |
| `app/Content/_Placeholder/Enemies/` | CREATE (directory) | Feeds PRD 03 (enemies) — placeholder art for the enemy types |
| `app/Content/_Placeholder/Abilities/` | CREATE (directory) | Feeds PRD 02 (abilities) — ability placeholder art (VFX, projectiles, etc.) |
| `app/Content/_Placeholder/TargetZones/` | CREATE (directory) | Feeds PRD 05 (levels) — target-zone placeholder art |
| `app/Content/_Placeholder/README.md` | CREATE (46 lines) | Policy doc: placeholder-first rule (quotes `MISSION.md:199-201` near-verbatim), marketplace-replacement sequencing, final-asset destination, subfolder-to-PRD mapping, and why `_Placeholder/` is used instead of Epic's `Developers/` folder |

## Acceptance criteria

- [x] **`app/Content/_Placeholder/` exists with subfolders `Characters/`,
      `Enemies/`, `Abilities/`, `TargetZones/`.** Confirmed via `ls -la`.
- [x] **README documents (a) the placeholder-first policy.** Quotes
      `MISSION.md`'s Definition-of-Done near-verbatim.
- [x] **README documents (b) that marketplace assets replace placeholder content
      only after the core loop is validated as fun, per REQ-3's sequencing.**
      Attributed to PRD 14 (Technical Architecture) as referenced in issue #58 —
      no fabricated PRD 14 file link, since no PRD 14 file exists in this repo.
- [x] **README documents (c) where final, non-placeholder assets should land
      instead.** Parallel non-`_Placeholder` folder structure, e.g.
      `Content/Characters/`, `Content/Enemies/`, `Content/Abilities/`,
      `Content/TargetZones/` — documented as a destination, not created now.
- [x] **No gameplay assets (meshes, sprites, blueprints) created by this change.**
      Verified — only directories and one Markdown file.
- [x] **`harness/ci.py` still reports `GATE_OK` (unaffected baseline).** Full mode
      run, not just `--quick` (see Validation below).

## Validation

```
HARNESS_START mode=full driver=cli
STATIC_SKIPPED no 'static' command in harness.config.json
UNIT_PASSED tests=19
APP_STARTED driver=cli
UE_AUTOMATION_RESULT passed=1 total=1
UE_AUTOMATION_OK
E2E_PASSED steps=1
HOLDOUT_ABSENT no .factory/holdout/run.py
MUTATIONS_ABSENT no harness/mutations/run.py
GATE_OK mode=full
```

Unaffected baseline, as expected — this change has no code footprint for the
harness to exercise. Content validation was performed manually (no automated
check exists for prose content): `ls -la app/Content/_Placeholder/` lists
`README.md`, `Characters/`, `Enemies/`, `Abilities/`, `TargetZones/`; README
covers all three required acceptance points (a)/(b)/(c); no `.uasset`/`.umap`
files created anywhere. Hard invariants reviewed by inspection against
`MISSION.md`'s 8 invariants — no governance files touched, no enemy/ability/
engine changes, `app/`-not-tracked invariant (#8) holds. No regressions.

---

The real Unreal project stays under `app/` (gitignored, D-003) — this changelog
is the tracked-repo record of that change per D-009. There is no matching
`app-source-tracked/` copy for this issue, since it contains no `.h`/`.cpp`/
`.Build.cs` source to mirror — only Content Browser folder scaffolding and a
README, both excluded from the mirror mechanism by design.
