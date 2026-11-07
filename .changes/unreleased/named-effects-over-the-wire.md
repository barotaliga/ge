---
type: Added
title: A mod's named effects reach remote players of Shooter's Edge
---

The game's own protocol extension (`*ge_ext` in the user info and the server info, as the z set travels) carries a mod's named point and trail effects to a remote client of the game's own (svc 120 and 121). A classic client never asks for it and never receives them, and recordings written to a file strip them, so every demo stays readable by classic players.
