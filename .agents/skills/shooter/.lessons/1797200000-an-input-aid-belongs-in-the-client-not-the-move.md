---
title: An input aid that needs memory belongs in the client's command, not in the move
date: 2026-11-10
scope: plugins/shooter/speedjump_assist.cpp, games/shooters_edge/client/listen.cpp, games/shooters_edge/net/classic_remote.cpp
concepts: [speedjump, easy-jump, auto-strafe, prediction, client-state, weave]
---

The easy jump first ran inside `player_move` (a server rule gated by the player's ask). Making
its hops weave left and right needed memory across commands (which side this hop turns), and
`MoveState` is what the server sends and the prediction replays: memory there either travels on
the wire or mispredicts. Synthesising the commands in the client (`assisted_command` with a
`SpeedJumpAid` kept beside the view) needs no server support, predicts exactly, works the same on
every server, and leaves the move classic to the bit. A drawn-only effect of the aid (the shown
swing) is a yaw offset on the drawn view, never on the sent aim.
