---
type: Added
title: Shooter's Edge's bots play on existing servers
---

A team of Shooter's Edge's bots joins an existing classic server (an mvdsv server running ktx, or ours) and plays a 4on4: `shooters_edge_bots --server=<host:port> --bots=4 --team=<name> --basedir=<your packs>` headless, or `shooters_edge --bots=4 --server=<host:port>` in the window, which draws the team's world as its bots believe it: the map, each bot's route, goal and the enemies it believes in, the item timers, the enemies the team's clients were sent, and each bot's plan. The console drives them: a line goes to every bot, `bot<N> <line>` to one (`bot1 skill expert`, `ready`, `status`). Each bot knows only what its own client is sent and what its teammates say. The team's picture records as a video (`--view=`, `--bots-view=`), each bot's session as a client recording (`--out=`). `-demovideo <mvd> <file> --autotrack` makes a match's video that follows the hottest player by kit and kill streaks.
