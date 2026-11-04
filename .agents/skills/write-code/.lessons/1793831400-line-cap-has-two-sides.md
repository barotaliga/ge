---
title: The line cap has two sides, the structure check and ge doctor; change both together
date: 2026-11-04
scope: tools/check-agents.sh, engine/cli/doctor.cpp, tests/cli_doctor_test.cpp
concepts: [line-cap, agents-check, ge-doctor, warning, allow-large-file]
---

The line cap is enforced twice: `tools/check-agents.sh` for this tree and `ge doctor`
(`kLineAdvice`, `kLineCap` in `engine/cli/doctor.cpp`) for a game repo, with the
`write-code` skill and its consumer copy saying the numbers. Past 1000 lines is a warning
that never fails the check; past 2000 fails. A warning is not a reason to split mid-task,
so a pull request that crosses 1000 by a few lines lands as it is.
