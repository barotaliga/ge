---
type: Added
title: Every match recorded by itself
---

The server core reads each match's phase from its game (warmup, countdown, match, intermission), keeps every change and says the phase in the status record a hub reads. With `sv_autorecord 1` a dedicated server records each match by itself to `demos/<map>-<date>-<time>.ged` in the game's folder, from its first frame to its last. Shooter's Edge's classic server reports its lobby's phases. Every recording a server runs (`--record`, its TV, each match's) now takes the same frame bytes, so running two at once on Shooter's Edge's classic server no longer splits its frames between them.
