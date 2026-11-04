---
title: A spawn function sees only the entities spawned before it
date: 2026-11-04
scope: games/shooters_edge/qc/campaign/, tests/shooters_edge_campaign_monsters_test.cpp
concepts: [qc, spawn-order, entity-text, think, find, teleport-train, path-corner]
---

The entity text spawns in order, so a spawn function that `find`s by `targetname` misses an
entity later in the text (a moving teleport destination before its path corners): start the
search on a think 0.1 s later instead of in the spawn function. A missile from a tall monster
spawns at `origin + view_ofs`; leave `view_ofs` at zero on a big body and its throw starts
inside the floor and dies on its first touch, so set it high on the body (the lava boss's is
96). A digest that hashes server entities (se-baseline, se-dedicated) moves when a mod gains a
function, since function numbers in `.think`/`.touch` shift: compare the lab's frags and
pictures against main before `--write`.
