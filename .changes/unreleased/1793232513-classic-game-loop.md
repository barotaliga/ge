---
type: Added
title: The classic game loop
---

`plugins/shooter` runs the classic game loop behind a game-program seam: the server's frame on the run clock, entities by movetype (toss, bounce, fly, step, noclip, pushers), thinking, touches, the area tree, world traces, a player's packet and commands with the speed check, and the client's commands and prediction with error smoothing; Shooter's Edge plays a scripted match on it that replays to the bit, shown by the new `se-loop` lab.
