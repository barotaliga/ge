---
title: A figure is sized by its model's first frame, never by every frame's reach
date: 2026-11-18
scope: games/shooters_edge/livemap/live_map.cpp, games/shooters_edge/livemap/icons.cpp
concepts: [live-map, figures, model-radius, atlas, draw-figure]
---

The live map draws each model at a fixed px a unit into a cell of the figure atlas, the cell sized
from the model's reach. Taken over every frame, the player's reach is its dead pose lying flat
(63 units against 35 standing), so every living player came out a third of his size and dark on
the floors: he looked missing. Size a figure from the first frame (a standing pose) and let the
rare pose that reaches further be cut at the cell's edge. The frames' `min`/`max` fields are not
a reach either: measure the positions. Draw the players' figures before the items', so the
frame's budget of new figures (`kNewFiguresAFrame`) never starves them behind turning items.
