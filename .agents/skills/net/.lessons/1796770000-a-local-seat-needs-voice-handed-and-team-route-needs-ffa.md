---
title: A listen server's own player hears voice by a callback, and a team route must cover a free-for-all
date: 2026-11-07
scope: games/shooters_edge/net/classic_server.cpp, games/shooters_edge/net/classic_game.cpp, games/shooters_edge/client/listen.cpp
concepts: [voice, local-seat, listen-server, voice-route, team-voice, free-for-all]
---

A local seat has no address, so what `relay_voice` writes into its datagram goes nowhere. Give
the non-bot local seat the voice bit, and hand it each message through
`ClassicServer::local_voice`. Its own talk goes in through `classic_local_voice`, which reuses the
relay. Before testing the plumbing, check the route itself. `VoiceRoute::team` with an empty team
skipped everyone, so every free-for-all was silent. Debug output in `relay_voice` showed each
message reaching the loop and being dropped there. The rule is now "same team, or neither has
one".
