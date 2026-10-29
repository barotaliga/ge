---
title: A GPU lab's deterministic metrics come in the same order on both paths, or no-adapter rows cut another digest
date: 2026-10-28
scope: engine/lab/labs/gpu_snow_lab.cpp
concepts: [lab, gpu, digest, placeholders, no-adapter, det-measured]
---

The digest hashes every deterministic metric and every shot's id in the order they are given.
`water_scene::placeholders` gives the shots' ids and then `gpu-errors` and
`pipelines-after-load`, so a GPU lab whose device path gives those two metrics before its shots
cuts a different digest on a runner with no adapter (Windows on CI) than on lavapipe. Give the
CPU-side metrics on both paths, mark every number read off the GPU `Det::measured`, and give the
two GPU counters last. Check by forcing the placeholder path once (a temporary env switch on
`if (!device)`) and comparing the two digests before committing the row.
