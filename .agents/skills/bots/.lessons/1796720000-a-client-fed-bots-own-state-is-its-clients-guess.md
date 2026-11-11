---
title: A client-fed bot's own state in se-team's hook is its client's guess, a link's delay off the server's
date: 2026-11-10
scope: games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/net/classic_team.cpp
concepts: [client-fed, mirror, prediction, vertical-speed, flights, on-command-hook, se-moves, why-log]
---

`se-team --client-fed=1` hands its per-command hook the world the fighter decided from: its
client's mirror, not the server's. Its own horizontal speed there is close to the truth (the
decision log's speed losses: 514 client-fed against 449 on the server's world, the same corners),
but its VERTICAL speed and the moment it leaves the ground are not: a flight cut by "vertical speed
0" read 76 % crashes, and the server's own world read at the hook is a link's delay behind the
command (a jump's rise showed 0.13 s late). Measure flights off the match's own recording by the
rule people's are read by (`move_study.hpp` + `shooter/hops.hpp`, as `flights_by_mode` does) and
join a bot's state to them by the clock (the recording's clock is the server's to 0.01 s).
