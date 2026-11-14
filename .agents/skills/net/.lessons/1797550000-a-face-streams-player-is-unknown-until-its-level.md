---
title: A face stream's player is unknown until its level has come
date: 2026-11-14
scope: games/shooters_edge/net/classic_tv_recording.cpp
concepts: [classic-tv, autotrack, multiview, pov, stream, lazy-view]
---

The first stream of `UdpTvMultiview` rides "the lowest slot playing" (`View::slot == -1`), and
which slot that is shows only in `face.streams[0].pov` once its server data has been read. Code
that asks "does a stream carry slot N yet?" before then finds none and turns on a second
conversion of the same player. Wait for `face.streams[0].level` before matching slots to streams
(auto track's `follow_pick` does).
