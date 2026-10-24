---
title: A GPU lab's no-device branch must take every shot id the device branch takes
date: 2026-10-24
scope: engine/lab/labs, games/fall_line/labs
concepts: [digest, no-gpu, placeholder, shot-id, windows, merge-queue]
---

`Lab::shot` hashes a shot's id even when its pixels are `Det::measured`. So a GPU lab whose
no-device branch skips a shot has a different digest on a row with no adapter. The merge queue's
Windows runner is such a row whenever Dawn fails to load its compiler DLLs. `menu-gpu` was ejected
from the queue twice because its placeholder path took each page's picture but not the page's
`-diff` picture. Write the no-device branch to take the same shots, in the same order, followed
by the same deterministic metrics. Check it by forcing the device to null in a local build
(`gpu.device.reset()` after `create_device`) and comparing the digest with the baseline, because
the container always has lavapipe and never takes that branch on its own.
