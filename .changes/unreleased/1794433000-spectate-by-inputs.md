---
type: Added
title: Watch a deterministic game by its inputs
---

`engine/replay/spectate.hpp` writes any play-contract game's run as a replay stream of held inputs, a digest a second and a keyframe save every few seconds, and a `Spectator` steps its own copy of the run to the player's digest. Through a TV tree it lets anyone watch Fall Line live: every viewer under every simulated network preset reaches the player's digest (the `spectate` lab).
