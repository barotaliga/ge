---
type: Changed
title: The classic games are called by their names in the code and docs
---

The engine's docs, code and checks now call the games Shooter's Edge plays by their names (Quake, QuakeWorld, Quake II, Quake III Arena, Doom; `q1`, `qw`, `q2`, `q3` and `doom` in code) instead of "the classic game" and "Quake II". The names never appear in what names our own product: `make agents-check` scans the build files for them.
