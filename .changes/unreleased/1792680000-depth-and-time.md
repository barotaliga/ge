---
type: Added
title: The depth prepass, the depth pyramid and TAA-U
---

The frame now draws the ground's depth first and shades each covered pixel once in an `equal`-tested pass that also writes a motion target. `engine/render/hiz.hpp` reduces the depth to a pyramid of farthest depths for culling, and `engine/render/taa.hpp` resolves a jittered frame, rendered at 25 to 100 % of the output's size, into a history at the output's size (Halton jitter, motion reprojection, a YCoCg clip box, a reactive mask, a reset on a cut). The `world` lab takes `--taa` and `--scale` and measures the shimmer and the overdraw; the new `taa` lab measures ghosting on a moving camera against the converged still.
