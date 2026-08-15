# docs/

Drop a PRD here to feed it into the factory. Convention, not a hard requirement —
`dark-factory-prd-to-issues` also accepts PRD text pasted directly on the command line.

```bash
# save/paste a PRD as docs/prd.md, then:
archon workflow run dark-factory-prd-to-issues --branch prd/my-feature "file:docs/prd.md"

# or skip the file entirely:
archon workflow run dark-factory-prd-to-issues --branch prd/my-feature "$(cat some-doc.md)"
```

That decomposes the PRD into discrete GitHub issues and files them — nothing more. It
does not triage them or decide what to build; every issue it creates goes through the
exact same `dark-factory-triage` gate a human-filed issue would. See the workflow's own
description and `.archon/commands/dark-factory-prd-decompose.md` for the decomposition
rules (independently-buildable units, concrete acceptance criteria, capped at 15
issues/run, deduped against existing open issues).

**Not yet built: pulling a PRD directly from Google Docs.** Today you copy the content
in yourself (paste it, or save it here first). Wiring up live Google Docs access for
headless (cron-dispatched) runs would need a persistent Google OAuth credential — a
real piece of infra (GCP OAuth client, one-time interactive consent, stored refresh
token) — that hasn't been set up. Worth doing once the text/file path has been used for
real; not built speculatively ahead of that.
