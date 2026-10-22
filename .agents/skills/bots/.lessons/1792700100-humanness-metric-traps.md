---
title: The first humanness metrics measured the wrong thing on a hold, a rest and a still axis
date: 2026-10-22
scope: engine/ai/human.cpp, engine/play/human.cpp
concepts: [humanness, hold, median, rest, lag, trigger, stick, bands, saturation]
---

Three traps found while calibrating `ai/human.hpp` against Fall Line's bot. A plain median of
holds is dominated by one-step holds (the bot nudges every step), so the hold is a TIME-weighted
median, and only part-way: a hand resting at 0 or pressed to an end is not "holding". A trigger
part-way is only passing through, so the hold's band is the stick's alone, and so is the
saturation's (a trigger at full is a person flat out, not robotic). And an axis whose wanted
never moves has no lag to find: the least-difference search returned 0 and flagged it, so a
wanted with no travel gives NaN. Before writing a band, run the metric on hands that should pass
(a person's numbers by assumption) and on a constant input.
