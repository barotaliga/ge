---
title: Channels steers a voice only by where it stands; place it to give it a gain and a pan
date: 2026-11-23
scope: engine/audio/channels.hpp, games/shooters_edge/client/demo_sound.cpp, plugins/classic/q2_loop_merge.hpp
concepts: [channels, merged-loop, gain, pan, pseudo-entity, move, al_merge_looping]
---

A slot's volume and attenuation are fixed when it is played; each `update` hears it again only
from its point (`move`). A voice whose gain and pan a game works out itself each frame (Quake II's
merged loops: every copy's ear shares summed, `q2_merge`) is played once at volume 1 and attenuation
1/1000 a unit, and moved each frame to the listener plus the pan along the listener's right plus
the rest straight up, at `(1 - gain) * 1000` units: the channels then hear exactly that gain and
pan. Stop it when the gain reaches 0 (it would sound at the edge of earshot otherwise), and give it
a pseudo-entity below the static loops' so no real entity's channel replaces it.
