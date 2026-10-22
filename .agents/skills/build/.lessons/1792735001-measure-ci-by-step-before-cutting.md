---
title: Read CI's time per step and per test before cutting anything
date: 2026-10-22
scope: .github/workflows/ci.yml
concepts: [ci, ctest, parallel, clang-tidy, labs-smoke, ccache, queue]
---

A 30-minute pull request was two serial loops, not too many rows: `make lint` ran clang-tidy
on one file at a time (29 min), and the debug ASan row's ctest ran its cases one at a time,
`labs_smoke` alone taking 10 of its 20 minutes. The job list's step times and the ctest log's
per-case seconds showed both in one look. The fix kept every row and every check: lint in
parallel over only what a change reaches, ctest in parallel with the labs cut into `--shard`
slices, and the test runner's `main.cpp` compiled once instead of once per test (clang-tidy
was linting it 55 times). Jobs that start minutes after the run do so because the account's
runners are busy with other pull requests: shorter jobs shorten everyone's queue too.
