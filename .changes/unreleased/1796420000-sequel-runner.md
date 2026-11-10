---
type: Added
title: The sequel's rules as a second runner
---

Shooter's Edge's server has a second rules runner, `sequel_runner` (`server/sequel_server.hpp`, form `"sequel"`), beside the QC server. It starts a version 38 map from its brushes (`MapSpec::brushes`), spawns its entities as data, places players at deathmatch spots or the start, moves each command through the sequel's player move, runs the rules' frame every 100 ms of host time, refuses `kill` for 5 s after a spawn and respawns the dead by attack after 1 s. It fills the same world every runner fills. It is held golden.
