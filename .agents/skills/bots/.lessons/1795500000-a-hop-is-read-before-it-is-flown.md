---
title: Read the pro's hop off a demo before tuning the bot's, and keep its hands exact
date: 2026-11-01
scope: plugins/shooter/hops.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/labs/se_hop_lab.cpp
concepts: [bunny-hop, hands, settle, demo-track, trip-timing, arc-check]
---

Tune a movement against a recorded player, not against intuition: `read_hops` on a multiview
demo gave the numbers the pilot's hop is held to (one ground frame between chained jumps, the
turn at the most the move allows, the side switched once a hop), and `se-hop` replays the
player's own trips from his start and speed so the bot's time sits beside his.

`ai::Hands`' spring is explicit Euler: a settle of two commands or less (0.03 s at 77 Hz) rings
or sticks on a stepping value. A move that needs every command exact (the hop's view on the
velocity) takes settle 0 and no delay; the human hop with a 0.15 s lead loses speed every hop.

Time a trip by its first PASS within reach of the end, not by `arrived` (standing on the node):
a hop at 450 units/s overshoots, turns and comes back, and the "arrived" time measures the
return. A seen-turn lead (the view led by the turn just measured) feeds back and oscillates;
lead by the model's turn rate. An arc check over 0.3 s with a wall brake fires all the time in
corridors: look 0.12 s ahead and only flip the side.

Without `PilotBot.world` the hop flies blind: in the arena an expert with no world picked up one
item in two minutes; with it, experts outscored novices 124 to 86.

A simpler hop for an average player (forward with the side key of the turn) steers by the wish's
angle off the velocity: at acos(30 / v) the air adds nothing, at 90 degrees it adds 30 across.
Pick the angle that gives the turn the route asks for. Forcing a minimum turn to gain on
straights (a gentle weave) and giving it the expert's arc check both made it SLOWER on the real
trips (walls and late hands); a side change swings this view 90 degrees, so change side only at
a jump.
