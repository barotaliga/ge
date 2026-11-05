---
title: A particle within the pool's soft band of a wall fades to nothing; a mark darkened by cover alone is a flat disc
date: 2026-11-05
scope: engine/render/particles.wgsl, engine/render/beams.wgsl, games/shooters_edge/client/world_draw.cpp
concepts: [soft-particles, soft-m, hit-dust, wounds, decal-relief, bullet-hole, burns]
---

`ParticleSettings::soft_m` (0.4 m by default) fades a particle by how far it is in front of what is
behind it, so a round's dust a few centimetres off a wall and a wound's speck on a body drew at a
few per cent: the effect was there in the births and the metrics and invisible in every picture.
Shooter's Edge's pool softens over 6 cm; a speck that must sit ON a surface is lifted toward the
eye past the band instead. Separately, the beams' burns darkened by the mask's cover alone, which
turns engine/detail's bullet-hole mask (a dark wall, a crater, a pale rim, cracks, a dust halo in
its relief) into a flat black disc; `BurnVertex::relief` darkens by the relief instead.
