---
title: A match mod's bots need bot frames from the first frame, and its next map needs the server's state carried
date: 2026-11-18
scope: games/shooters_edge/server/qvm_server.cpp, games/shooters_edge/server/qvm_calls.cpp, games/shooters_edge/server/rules_runner.cpp, games/shooters_edge/net/qw_game.cpp
concepts: [add-bot, bot-frames, blocked-handler, carry-server-state, map-change, spawn-parms, created-cvars, mod-console]
---

A compiled match mod reads its bot routes on its 20th bot frame, so bot frames run on every
server frame from the map's start whether or not a bot plays; running them only with a bot
present left every bot standing on its spawn. `add_bot` must try each free slot (the host's seat
hook may refuse the first) and answer the slot, never 0.

A mod that keeps its mode in its own cvars, localinfo and serverinfo loses it at a map change
when the next map's rules are made new: `carry_server_state` copies them between runners of the
same form, but never a setting the server's config names (`ServerConfig::rules`): a vote's
change put back at the map's end was carried again until it was skipped. The spawn parms go
through the endpoint (`QwClient::parms`) and back on `begin`.

A config's `set name value` lines for a name no layer knows (the mod's switches) reach the module
only if the server hands them over (`ServerConfig::created`); a `+set` on the command line alone
made nothing the module could read.
