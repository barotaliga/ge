---
type: Fixed
title: Joining stock QuakeWorld servers
---
Joining from the command line (`+connect`) now sends the player's model checks and the map's
checksum, so a server that checks them lets the player in. A map, model or sound the player lacks
is fetched before the game asks to spawn, and the reason a server gives for dropping the player is
shown in the console. A server that plays another version of a stock map (the rerelease rebuilt
them) is joined with its own version: the client fetches it, keeps it hidden beside the player's
copy by its checksum, and remembers which version each server took. Quake II's "entered the game"
is said once on a new map.
