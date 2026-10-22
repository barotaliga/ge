---
title: Rebuild the labs binary before comparing its digest with a test's
date: 2026-10-22
scope: games/fall_line/sim, games/fall_line/labs, tests/labs_digests.txt
concepts: [digest, stale-build, labs-binary, debug-release, determinism]
---

A test target and the `labs` executable link the same `fall_line_sim` library, but building one
does not rebuild the other. After a change to the sim, a `labs` binary that was built before the
change still prints the old run's digests. Read beside a freshly built test or a release lab,
that looks like the debug and release builds disagreeing, which is a determinism scare that
costs a step-by-step trace to clear.

Before comparing a lab's digest with anything, run `cmake --build build/<preset>` with no target,
or name `labs` explicitly. Then rerun both sides. A real debug/release split survives a clean
rebuild of both; a stale binary does not.
