---
title: Ask a game for its broadcast frame once a frame, and start a match's recording a frame late
date: 2026-11-15
scope: engine/server/record.cpp, engine/server/autorecord.cpp, games/shooters_edge/net/classic_game_tv.cpp
concepts: [tv-frame, server-record, autorecord, phase-events, keyframe, map-restart]
---

Shooter's Edge's classic `tv_frame` hands out the multiview records written since the last call, so
two recordings on one server (the `--record` file and the TV, or a match's) each calling it got half
a frame. The core now asks once a frame and shares the bytes (`Server::tv_frame_bytes`), and
`record_begin` takes the frame's bytes before its keyframe, so a recording begun on a running
server keys off this frame, not the one before. A phase read after a frame is what the game decided
at the frame's end: the classic lobby restarts the map for the match AFTER the frame's records were
written, so a recording begun on that frame keys off players the live server has already dropped.
Begin a match's recording at the frame after the turn (`MatchRecorder::due`); the
`se-dedicated-lobby` lab's second match went from 5 wrong views to none.
