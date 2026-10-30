---
title: make precheck reads tracked files only, so a new file's libm calls pass it and fail CI
date: 2026-10-28
scope: tools/check-libm.sh, tools/precheck.sh, engine/lab/labs
concepts: [precheck, libm-check, git-ls-files, untracked, labs, determinism]
---

check-libm lists files with `git ls-files`, so a lab written in this session and not yet
committed is never scanned: precheck reported libm ok and CI's gates failed on the new lab's
`std::sin`/`std::log`. Labs are simulation code for this check (their digests must match on every
row), so call `ge::core::libm` and `ge::core::hypot` in them from the start, and `git add` new
files before running `make precheck`.
