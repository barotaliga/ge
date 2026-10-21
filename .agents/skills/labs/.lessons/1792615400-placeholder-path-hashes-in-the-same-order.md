---
title: A GPU lab's no-adapter path must make its deterministic calls in the device path's order
date: 2026-10-21
scope: engine/lab/lab.cpp, games/fall_line/labs/gpu_world_lab.cpp
concepts: [digest, gpu-lab, placeholder, no-adapter, call-order, windows]
---

The digest is one running hash over every deterministic metric and every shot's id, in the
order they are recorded. A GPU lab with no adapter records placeholders so its digest matches
the rows that have one, but the same values in a different order still give a different digest.
The world lab recorded `gpu-errors` before its placeholder shots and after its real shots, so
the Windows row (no adapter) failed `labs_smoke` while Linux passed. Write the no-adapter path
as the same sequence of calls as the device path. To check it locally, force that branch
(`if (true)`), run `labs <id> --smoke` and compare the digest with the baseline.
