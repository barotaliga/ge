---
type: Added
title: A game's player CLI builds and runs QC when the game links the progs plugin
---

`secli qc build|disasm|run|trace|profile` work for a mod's author who has Shooter's Edge and not the engine: the progs plugin's tool runs in secli's own process, and its answers name `secli`. The `qc` rows now live once in `engine/toolkit/qc.hpp`; `ge qc` runs the same rows by building the game repo's `progs` executable. A toolkit row whose body is a plugin's names its tool (`Row::tool`), and a game's CLI offers it only when its `Spec::tools` lists that tool.
