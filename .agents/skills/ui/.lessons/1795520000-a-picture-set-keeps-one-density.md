---
title: A swapped picture joins its set at the set's density, never at its own
date: 2026-11-02
scope: games/shooters_edge/menu/classic_art.cpp, games/shooters_edge/hud/classic_art.cpp, games/shooters_edge/client/classic_adapter.cpp
concepts: [density, replacement-picture, upgrade, glyph-sheet, scaled-nearest, menu, hud]
---

The menu's and the HUD's loaders take the set's density from the first picture they upgrade (the
glyph sheet, the bar) and draw every picture at that many pixels a classic pixel. A copy that comes
back at `k` times the pack's size is used when `k` divides the density (scaled up nearest by
`density / k`); any other copy is dropped for the pack's own scaled up, so one set never mixes
sizes. So a player's glyph sheet at twice its size doubles the whole set, and a plaque at its own
size still shows in it. The upgrade hook returns 1 for a same-size picture file; returning 0 means
"keep the pack's".
