---
type: Fixed
title: The CPU sampler always records a start and a stop sample
---

`prof::CpuSampler` now takes its first sample even when `stop()` arrives before its thread first runs, so every run has at least two samples to read a span from. On a loaded machine, a short run could come back with only one.
