---
title: make libm-check flags any function called log, exp or pow in a scanned directory
date: 2026-10-22
scope: tools/check-libm.sh, engine/core, engine/lab
concepts: [libm-check, naming, log, false-positive, output]
---

`tools/check-libm.sh` matches `log(` (and `exp(`, `pow(`, `sin(`, ...) not qualified by another
namespace, so a function of our own named `log` reads as a call to the platform's logarithm.
The log channel's writer is `output::log_on` for that reason. Name a function in `engine/core`,
`engine/lab`, `engine/audio`, a plugin's simulation or any `labs/` directory so it is not one
of the transcendentals' names, or the check fails on CI.
