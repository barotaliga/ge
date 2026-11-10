---
type: Added
title: Shooter's Edge's dedicated server runs a match mod shipped as a compiled module
---

A mod folder whose packs hold a `qwprogs.qvm` now runs it on `shooters_edge_server --mod=<folder>`: the module starts the map itself, answers clients' commands first and runs the match through its own exports, and classic clients join it unmodified (its about page, scoreboard, `ready` and countdown work). The new `se-qvm` lab reports a module's exports, system calls, entities and faults on any map.
