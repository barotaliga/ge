---
title: A hop's reversal: the key swaps with the side, and the hand turns back on it
date: 2026-11-18
scope: plugins/shooterbots/bot.cpp, games/shooters_edge/labs/se_bhop_lab.cpp
concepts: [bunny-hop, airkeys, hopside, keyswap, mouse-hand, turn-back, se-bhop, air-strafe]
---

Read on the hop course (`se-bhop`): under `airkeys` + `hopside` a hop's air held NO key 29-36 % of
the time. At each reversal the key is let go while the view crosses the velocity (a key that
would brake for a moment loses to none), about 11 commands at the hand's pace, and nothing turns.
Holding the pilot's side key the whole flight (`keyswap`) helped only once the mouse hand turned
back with it: in the hop's air the hand runs at ease, so a stroke the other way waits until the
one under way is 90 % done (`kTurnBackAtEase`) and the view sits 9 degrees past the velocity,
where the air's 30 cap adds nothing. Cut at 30 % (`kSwapTurnBack`), the expert's lap went 25.8 ->
21.7 s, walls 5 -> 1.5. The key following the mouse's own turn instead was worse (31.7 s): the
velocity then chases the hand's jitter. Not for the simple hop (an average's: one side a flight,
forward with it): the whole flight on the bare key turns it into the walls.
Also: `core::angle_diff(a, b)` is RADIANS and returns b - a; a degree difference is written out.
