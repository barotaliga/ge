---
title: A move told another game's numbers feels like it only with the landing pause too
date: 2026-11-14
scope: plugins/shooter/pmove.cpp, plugins/shooter/sequel_move.cpp, games/shooters_edge/profiles/remix_arsenal.hpp, games/shooters_edge/labs/se_remix_moves_lab.cpp
concepts: [classic-remix, cr-moves, landing-pause, strafe-jump, air-accelerate, sequel-move, pmove]
---

Run speed, friction and the air rule alone made the classic move told the sequel's numbers gain
MORE than the classic one (the whole-wish air at 1 out-gains the capped-30 air at the best angle);
what holds the sequel's hops down is its landing pause (a landing past 200 down blocks the jump
0.144 s), so `pm_landpause` went into the classic move and `land_pause` became a knob of the
sequel's. In the classic move the landing must be checked where the ground is first found, as a
command starts (the jump runs in that same command) and after the slide, with the fall speed taken
before the slide clips it. A sequel test floor needs air inside the map's bounds (a ceiling brush):
outside the bounds is solid, and a SequelMove whose `cmd` is never assigned stands still without a
word. Packed sequel origins stop at 4096 units, so a long run starts in the floor's middle.
