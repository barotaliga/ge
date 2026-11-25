---
title: The own missiles and jump come off the shown command, from the move's state on that command
date: 2026-11-23
scope: games/shooters_edge/net/qw_remote.cpp
concepts: [predicted-projectiles, own-missiles, jump-sound, shown-command, handover, smoothing, predict-player]
---

Throw a predicted missile and play a predicted jump only when the weapon's shown command first
reaches its command, the same rule the shots' sounds follow: the weapon pass hands each new shot
out (`RemoteWeapon::fired`, with its command), and `predict_player`'s `each` keeps the state after
every command so the launch uses the player's place on that command (not the newest one, a
command later) and a jump is the command whose `jump_held` turned on from the ground. Date a
missile back by the msec of the commands sent after its own. Throw none when the server's ping
byte is 0 (a game on this machine: its copy is not late). Draw stand-ins beside the server's
entities (`DemoState::own_missiles`, numbers above any server's) so `demo_placed` and
`demo_fx_frame` draw them and their trails without knowing they are predicted. A shared
`sh::Smoothing` is in the simulation's digests (`sim/match.cpp`): put a client's finer rules
behind fields whose defaults keep the old bits. Measure with `se-own-ahead`.
