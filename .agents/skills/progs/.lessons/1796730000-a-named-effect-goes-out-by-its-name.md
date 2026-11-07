---
title: A mod's effect number stands for its name on the server only; send the name
date: 2026-11-06
scope: games/shooters_edge/server/bi_message.cpp, games/shooters_edge/client/listen.cpp, games/shooters_edge/net/classic_frame.cpp
concepts: [pointparticles, trailparticles, particleeffectnum, dp-sv-pointparticles, net-event, classic-fallback, named-effect]
---

`particleeffectnum` hands out numbers per server, so the events (`point_effect`,
`trail_effect`) carry the effect's NAME, and a client looks it up in its own `particles/*.fx`
book. A classic client must never get a message number it does not know (the connection drops):
a classic name maps to its temporary entity (`classic_temp_entity`, shared by the frame and the
recorder), anything else is dropped and counted. On the listen server, a test or a lab can feed
the window an event by pushing a `NetEvent` onto `l.server->out.events` before `listen_frame`:
the frame's tap reads it before the sink is cleared (`se-weapon-fx --mod-effects=1` does this).
