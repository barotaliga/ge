---
title: An aid the server applies to one command puts back the view it turned, after the frame
date: 2026-11-12
scope: games/shooters_edge/server/rjump_rules.cpp, games/shooters_edge/server/qc_server.cpp, games/shooters_edge/server/sequel_server.cpp, games/shooters_edge/client/listen.cpp
concepts: [rjump, rocket-jump, aid, forced-view, view-angle, server-side, readiness]
---

The rocket jump helpers rewrite one command on the server (the view turned 80 degrees down or
half round, attack and jump held), unlike the easy jump, which lives in the client: the server
must gate them and only the rules know when the launcher can fire. The first cut left the
player's `v_angle` at the turned view after the frame, and the window's listen frame reads a
`v_angle` off the angles it sent as the mod forcing the view (a spawn, a teleport), so the
player's own screen snapped to the floor. Put the view back after the runner's frame (the QC
runner: the command's own angles, clamped to the pitch limits; the sequel's: its view before the
command), so only the shot sees it. Also: `plugins/shooter/rocket_jumps.hpp` already exists (rocket
jumps read off recordings); grep a type's name across the plugin before choosing one.
