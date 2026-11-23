---
type: Added
title: A game's spelling of a setting may write its own values
---

A setting a classic game writes with other values is now one setting too: the game's spelling maps each of its values onto ours, and setting it prints both, for example `cl_predict 0 (cl_nopred: 1)`. Quake II's `cl_predict` and `cl_gibs` now set `cl_nopred` and `cl_gibfilter`, so a player's choice holds in every game.
