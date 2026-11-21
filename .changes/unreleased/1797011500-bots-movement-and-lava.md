---
type: Changed
title: Shooter's Edge's bots move more like people and stay out of the lava
---

The bots' movement and map sense were reworked against people's recordings, each behaviour a switch judged head to head (`plugins/shooterbots/features.hpp`): hops copied from people's starts, the jump held for the landing frame and no forward key in a hop, the circle jump, gap jumps over water and lava, rocket jumps fired only off the ground with the launcher ready, a brink guard that flies the player's box along its run and brakes before a fall into lava or slime (dm2's lava deaths about halved), the floor trap pressed only with an enemy on it, bridges and floor doors seen gone kept off the routes, plats boarded at rest, and place names read off the map's loc file. The `se-team` lab's trace gains the pilot's link, keys, liquids, brink, the trap and the hop's side; `se-reach` stages gap jumps, rocket tricks and walks into a pit in seconds; `se-spot` draws a place every way; `se-recipes`, `se-approach` and `se-trick` read people's moves, item approaches and trick maps.
