---
title: A demo drawn in two places must share one placement, or one of them drops the players
date: 2026-11-01
scope: games/shooters_edge/client/main.cpp, games/shooters_edge/client/demo_video.cpp, games/shooters_edge/labs/gpu_se_timedemo_lab.cpp
concepts: [demo, playdemo, timedemo, demovideo, demo-placed, world-models, model-list, gamedir]
---

`-demovideo` drew a demo's entities (`demo_placed` into `FxFrame::models`, the world made with the
demo's model list) while the window's `playdemo`/`timedemo` and the `se-timedemo` lab passed an
empty `FxFrame{}` and no `ModelLoad`, so players were never seen in the window. The fix shares
`demo_placed` (client/demo_video.hpp) and makes the world again whenever the demo's model list
length changes (a client recording's list grows after the level starts). When one feature is drawn
by the export and by the window, grep both for `create_world_draw(` and `draw_world(` and check
they pass the same models; and every `rebuild_world` of the listen game must pass
`listen.server->models`, or the world after a demo (or the campaign's next map) loses its models.
A multiplayer recording's serverdata names its game folder (`DemoState::game`); a team mod's demo
needs that folder mounted (`demo_game_dir`). Check with a lab picture where a player is in view:
`se-timedemo --format=match --stage-pak=fixture`.
