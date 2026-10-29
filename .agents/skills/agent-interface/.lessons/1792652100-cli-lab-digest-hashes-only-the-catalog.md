---
title: A ge lab hashes only what no release or tag moves
date: 2026-10-22
scope: engine/lab/labs/cli_loop_lab.cpp, tests/digests
concepts: [cli-loop, digest, labs-smoke, determinism]
---

The first `cli-loop` digest differed between two smoke runs and between branches: `ge help`
prints the engine's version, `ge new` and `ge find` print paths and read the engine's tags,
and a table with milliseconds went into the hash. Only the catalog's shape (`catalog.commands`,
`catalog.flags`) is deterministic; every answer's lines, bytes and time are `Det::measured`,
and the lab still fails an answer over its line budget. The catalog's shape was hashed at first,
but then every PR adding a command or a flag moved the one `cli-loop` digest and two such PRs
clashed in the merge queue, so it is measured too and the lab's digest no longer moves.
