---
type: Changed
title: Shooter's Edge plays the jump's sound, always runs, and lowers the weapon on death
---

A jump is heard again (`player/plyrjmp8.wav` from the game's own packs), and a swimmer pushing up splashes. Nobody walks: `cl_run` is on by default and no key is bound to `+speed` (SHIFT no longer walks). `+moveup` and `+movedown` swim up and down, bound to no key: the classic game has no crouch, though the sequel's move in the shooter plugin ducks on `+movedown`. When the player dies, the held weapon sinks out of view over 0.6 s instead of hanging in front of the dead player's eyes; recorded games and joined servers show it too.
