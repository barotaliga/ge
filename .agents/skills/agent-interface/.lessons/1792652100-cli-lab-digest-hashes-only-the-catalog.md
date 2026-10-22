---
title: A ge lab hashes only what no release or tag moves
date: 2026-10-22
scope: engine/lab/labs/cli_loop_lab.cpp, tests/labs_digests.txt
concepts: [cli-loop, digest, labs-smoke, determinism]
---

The first `cli-loop` digest differed between two smoke runs and between branches: `ge help`
prints the engine's version, `ge new` and `ge find` print paths and read the engine's tags,
and a table with milliseconds went into the hash. Only the catalog's shape (`catalog.commands`,
`catalog.flags`) is deterministic; every answer's lines, bytes and time are `Det::measured`,
and the lab still fails an answer over its line budget. Adding a command or a flag moves the
digest on purpose: rewrite its line in `tests/labs_digests.txt` in the same PR.
