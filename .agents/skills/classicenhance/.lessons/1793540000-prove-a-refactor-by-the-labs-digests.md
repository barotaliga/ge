---
title: Move a hard-coded path into the profile and prove it by the labs' digests, not by reading
date: 2026-11-01
scope: plugins/classicenhance
concepts: [profile, content-profile, refactor, digest, before-after, asset-kind, sound-path]
---

Moving the first game's paths into `kProfiles` touched every stage. The proof that nothing moved
was the `enhance`, `classic-menu` and `classic-weapons` labs run at `--smoke --tag=before` on a
stash of main, then `--compare=before` and their digests after: every picture's diff 0.000 and
every digest the same. Watch the stages that cut a path by a fixed count: the sounds kept their
extension (`substr(6)` past `sound/`, so the cache's `sounds/<path>.wav`), while the maps and the
pictures drop it; `asset_name` drops both, so a stage that keeps the extension cuts by
`kind.dir.size()` instead.
