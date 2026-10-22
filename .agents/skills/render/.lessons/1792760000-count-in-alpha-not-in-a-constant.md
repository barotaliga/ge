---
title: Let a mip chain carry its own normaliser in alpha
date: 2026-10-22
scope: engine/render/bloom.hpp
concepts: [bloom, mip-chain, normalise, alpha, levels, resize, energy]
---

The bloom's up chain sums one level per step, so the post pass has to divide by the number of
levels to keep the mix energy-conserving. That number depends on the output's size (a small
target has fewer levels), which a pipeline constant fixed at load cannot know after a resize.
Each down level writes alpha 1 and each up level adds its alpha to the tent of the level below,
so the top level's alpha is the count and the post pass divides by it. Use the same trick for
any accumulated chain whose length changes with the target: the data says what it summed.
