---
title: Precheck's own test batch can still fill the disk; set PRECHECK_TEST_BATCH=20 by hand
date: 2026-11-14
scope: tools/precheck.sh
concepts: [precheck, disk-full, test-batch, asan, link, no-space]
---

A change that reaches about 200 tests (a header every classic net file includes) had precheck
pick a batch of 144 from 9 GB free (16 a GB): the debug ASan test executables are larger than
that guess, the links died with "No space left on device" and lld's bus errors, and the gate
reported 144 red tests that were only a full disk. Delete the test executables
(`find build/debug/tests -maxdepth 1 -type f -name '*_test' -delete`) and run
`PRECHECK_TEST_BATCH=20 make precheck`: 206 tests then ran green in ten batches.
