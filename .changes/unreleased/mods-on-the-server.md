---
type: Added
title: C++ mods run on the real server
---

A C++ mod package now stacks on a Shooter's Edge server's rules, not only on the scripted match: name its folder in `sv_mod` (`--mod=<folder>`, comma-separated with game folders) and the dedicated or listen server loads it and runs its hooks after the rules' own on every map, on top of a bytecode program or a compiled module. Its log lines reach the server console as `mod <name>: ...`, and its run is part of the server's digest, so a modded server replays to the bit. A sequel map or a coop campaign refuses a package for now, saying so. The baseline mod's QC sources are frozen: our own rules are being carried over to C++ packages.
