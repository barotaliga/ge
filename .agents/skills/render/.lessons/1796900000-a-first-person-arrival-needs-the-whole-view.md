---
title: A first-person arrival needs the whole view; particles at the eye read as a few specks
date: 2026-11-09
scope: engine/render/post.wgsl, games/shooters_edge/client/world_fx.cpp, games/shooters_edge/labs/warp_film.hpp
concepts: [arrival, teleport, spawn, first-person, post-warp, screen-effect, r_arrival, live-uniform]
---

Ash blown past the eye (kept off it so it never fills the near plane) covered the middle of the
screen and nothing else; the player said a teleport did not feel like one. A first-person moment
that should take over the senses belongs in the post pass, over every pixel: the arrival
(`arrival_warp`, `set_post_warp`) is a flash, a pull toward the middle with radial streaks, and a
clear disc opening past the corners. Put such a moment in the post's uniform block behind one
uniform branch rather than an `override`: it is on for 0.65 s at a time, and remaking the
pipeline to turn it on would stall the frame it is needed in. Film it (`se-client --game --warp`
writes `warp-eyes-<view>.gif`); the before film is the same build at `r_arrival 0`. Building the
whole debug tree no longer fits the disk; build only the targets you need
(`cmake --build build/debug --target labs render_post_test`).
