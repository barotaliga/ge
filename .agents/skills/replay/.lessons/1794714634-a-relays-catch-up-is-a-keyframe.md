---
title: A stream that already has a late viewer's catch-up has its keyframe for free
date: 2026-11-15
scope: games/shooters_edge/net/classic_game_tv.cpp, games/shooters_edge/net/classic_mv_state.hpp, engine/server/record.cpp
concepts: [keyframe, catch-up, multiview, watch-hooks, player-view, server-record]
---

The classic game's server-core broadcast took no new codec: its frames are the multiview records
a relay already takes, and its keyframe is `mv_catch_up` off an `MvState` fed the same records,
the same thing a relay writes for a viewer who joins late. Write one frame at the first keyframe
(`mvd_frame` at once), or the keyframe at frame 0 is empty and frame 0's views cannot match.
Write a player's view through the stream's own encoders (`write_coord`, `write_angle16`) on both
sides, live and kept, so the quantising is the same and the bytes compare exactly. The endpoint
writes the streams, so an offline server (no port) records nothing.
