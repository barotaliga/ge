---
title: A box sized from a text's advance can be a hair narrower than the advance; fit with slack
date: 2026-10-24
scope: engine/ui/console.cpp
concepts: [font, advance, fit, float, ellipsis, layout]
---

The console's suggestion list sizes its box from the widest detail's advance, then fits each
detail into what is left of the box. Rebuilding that width from the box's edges in float lost a
fraction of a pixel, so the widest detail ("= 0") no longer fit and was cut to "=...". When a
width is derived from the same advance it is compared against, compare with half a pixel of
slack (or keep the measured width rather than re-deriving it from edges).
