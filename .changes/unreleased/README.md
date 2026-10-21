# Unreleased changes

One file per user-visible change, named `<unix-ts>-<slug>.md`:

```markdown
---
type: Added        # Added | Changed | Deprecated | Removed | Fixed | Security
title: Short title # optional
breaking: true     # optional; forces a major bump (a minor bump while on 0.x)
---

One-sentence summary of the change, written for a consumer of the engine.
```

The release workflow turns them into a `CHANGELOG.md` section and deletes
them. `python3 tools/release.py check` validates them; `bump` previews the
next version. See `docs/publishing.md`.
