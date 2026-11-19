---
title: A game whose wire speaks for one viewpoint records every slot's state whole
date: 2026-11-15
scope: games/shooters_edge/net/q2_game_tv.cpp, engine/server/record.cpp
concepts: [server-record, whole-state, keyframe, watch-hooks, player-view, q2-game]
---

Quake II's wire is written per client, so there is no multiview stream to reuse as the
broadcast. Write each frame as a view for every slot off the rules' world rows and let the keyframe
be the same bytes: the watch keeps the last frame, a seek needs no earlier keyframe, and the replay
file's zstd packs the repeats (about 1.25 kB a second for two players, under a third of the classic
stream's). Use one writer for the live player_view and the frame so the bytes compare exactly, and
it records offline too, since it reads the rules, not a network endpoint.
