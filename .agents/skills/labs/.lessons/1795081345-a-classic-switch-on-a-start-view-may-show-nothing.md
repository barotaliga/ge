---
title: A switch about figures or lamps needs views near them; the se-client start views can show 0 %
date: 2026-11-19
scope: games/shooters_edge/labs/gpu_se_client_lab.cpp
concepts: [se-client, before-after, views, ce-switches]
---

`se-client --before="ce_shadows 0"` on q2dm1's default views changed nothing: the sequel's maps are
lit by their lightmaps and the figures' shadows only show near a picked lamp. Pick `--views`
(`x,y,z,yaw,pitch;...`, map units, pitch down positive) near items and lamps, and check the
before/after diff per view before publishing. `addbot` in the sequel needs a lobby the lab does not
start, so put the camera by items rather than waiting for players.
