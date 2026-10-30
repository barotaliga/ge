---
title: New ge commands move the cli-loop lab's digest
date: 2026-10-29
scope: engine/cli/commands.cpp, tests/digests
concepts: [ge-qc, catalog, cli-loop, digest, labs-smoke]
---

Adding the four `ge qc` rows to the catalog changed `cli-loop`'s digest: it counts the catalog's
commands and flags (57 to 61 commands). That is an intended move. Rewrite that row along with
your own lab's, run `build/<preset>/labs cli-loop --smoke --check-digests=tests/digests` to
confirm, and say why in the PR. (`ge qc build` in M18d left the digest as it was: check, never
assume.)
