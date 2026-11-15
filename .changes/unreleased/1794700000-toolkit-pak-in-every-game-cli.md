---
type: Added
title: Every game's player CLI runs the pak commands ge has
---

`engine/toolkit/` (`ge::toolkit`) holds the command groups `ge` and every game's CLI share, written once. The first is `pak` (`ls`, `extract`, `build`, `convert`, `sum`): `secli pak ls pak0.pak` now works for a player or a modder who has the game but not the engine, and its answers name `secli`, never `ge`. The answer shape (`Outcome`, `Finding`) moves from `engine/cli` to the toolkit with it.
