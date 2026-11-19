---
title: An effect drawn ahead of the server needs the server's time, put in the rules' world
date: 2026-11-19
scope: plugins/classicenhance/hologram.hpp, plugins/shooter/rules_world.hpp, games/shooters_edge/client/world_models.cpp
concepts: [ce-respawns, hologram, item-back, back-in, rules-world, protocol-extension, morph-shell]
---

Drawing an item before it respawns (`ce_respawns`) needs a fact no classic wire carries: when it
comes back. Put such a fact in the rules' world row (`WorldEntity::back_in`) where every server
fills it from its own state (the program's hidden, unowned, item-flagged entity with a later think;
Quake II's `respawn_at`), so the window reads one place for either game; a joined client gets it
by a ge-set extension (`item-back`, svc 119) that a classic client never asks for. Time the effect
to end when the real item shows in the STATE: the window draws `owed` behind it, so subtract
`min(owed, tic)` from the seconds left. A morph shell pushed out along the normals swells a small
model into a blob: keep the push small (0.5 units) and its alpha faint, or the shape is lost. In a
test, set the loop's `Edict` copy (flags, nextthink, modelindex) as well as the program's fields: a
frame copies the edict back over them.
