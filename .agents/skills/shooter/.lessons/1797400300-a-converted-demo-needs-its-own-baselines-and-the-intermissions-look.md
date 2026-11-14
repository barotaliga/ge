---
title: A demo converted to another protocol deltas from the baselines it wrote, not the source's
date: 2026-11-14
scope: games/shooters_edge/client/demo_convert_single_out.cpp
concepts: [demo-convert, mvd-to-dem, baseline, player-slots, intermission, colours, se-demo-convert]
---

When a converter renumbers entities, a source baseline it could not place (a player whose target
slot is not known until the first frame) is left out of the target, so every delta for that
entity must start from nothing, not from the source's baseline: deltas from the source's made the
teammates in a converted `.mvd` → `.dem` draw with model 0 (invisible) while every number in the
lab still matched, because the lab compares the eye, angles and status, not the entities of a
cross-format pair. Look at a picture (`se-demo-video` on both files, side by side) before calling
a converter exact. Two reader gaps turned up the same way: in the intermission a single-player
demo views from the view entity's own origin and angles (the reader froze the last eye) and the
multiplayer readers must keep the intermission message's angles (they took the player's or the
command record's). The `se-demo-convert` lab pairs a `.dem` target's frames by time (gaps over a
tenth of a second are filled), with the clocks' offset taken where the eye first moves.
