---
title: Hold a generator with a self-written digest golden, a build-twice check and rule invariants
date: 2026-10-21
scope: tests/mountain_resort_build_test.cpp
concepts: [golden, determinism, digest, invariants, generator]
---

A generator with no outside reference is held three ways in one test. Each attempt is built
twice, and the two section lists must be equal, which catches state read from outside the
attempt. Each row is compared with `tests/golden/mountain_resort_build.txt`, which the test writes
itself under `GE_GOLDEN_WRITE=1`, so a change that moves a seed is visible and the PR has to
say why. The rule book is checked on what was built: every run is a line on the map, R27's
count holds, and R29's reckoning passes on every piste kept. Use `CHECK_MSG` with the key of
the failing attempt and the function to read next. Build the message only when the check
fails, because a per-point string costs more than the check.
