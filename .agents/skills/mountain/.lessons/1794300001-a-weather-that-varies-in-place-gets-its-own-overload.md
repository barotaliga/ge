---
title: A weather that varies over the map gets a placed overload, and the placeless one keeps its bits
date: 2026-10-30
scope: plugins/mountain/air/air.hpp, games/fall_line/app/frame.cpp, games/fall_line/hud/snapshot.cpp
concepts: [air, wind-at, fall-at, storm, determinism, digest, overload]
---

`air::wind_at(level, t)` and `air::fall_at(level, t, y)` are one wind and one fall for the whole
map, and every dealt map's labs and tests hold their bits. A thunderstorm's gust front and rain
differ from place to place, so they came in as overloads that take the storm and a place
(`wind_at(level, storm, t, x, z)`, `fall_at(level, storm, t, x, y, z)`) and start from the
placeless answer, adding nothing when the storm has no cells. Every reader that should feel the
storm (the frame at the eye, the HUD at the skier) moved to the placed call; nothing else moved,
and no digest did. Lay the storm once (`storm_of`) and keep it beside its level pointer and
strength, rather than laying it in each call.
