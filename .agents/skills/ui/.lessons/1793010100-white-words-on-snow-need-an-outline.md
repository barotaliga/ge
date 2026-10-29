---
title: A drop shadow alone does not carry white HUD words over white snow; outline them too
date: 2026-10-23
scope: engine/hud/face.cpp
concepts: [hud, legibility, shadow, outline, snow, contrast]
---

The first `hud-kit` pictures put white words with a 2 px navy drop shadow over a daylit
snowfield: the big TrueType speed all but vanished, because only one edge of each stroke had
anything dark beside it. `draw_shadowed` now draws four copies a little over half the drop off
each way (an outline) before the drop, and the words read over white and over a dark corridor
alike with no plate behind them. Judge a HUD's legibility over the brightest backdrop it will
meet, not over the game's usual mid-blue snow.
