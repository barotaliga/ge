---
type: Added
---
Shooter's Edge has a campaign host for single-player programs. It plays a chain of maps and carries every player's spawn parms and the program's `serverflags` (the runes) across `changelevel`. `restart` replays a map with the parms it was entered with, and `map` starts a new game. At each map's start the host rounds and clamps `skill`, sets `registered` from the content, and writes the program's `deathmatch`, `coop` and `teamplay`. The loop gains a single-player clock: the time advances after physics, an entity thinks at most once a frame, and two settling frames run before anyone joins. The QC compiler now marks a program with the layout form it matched.
