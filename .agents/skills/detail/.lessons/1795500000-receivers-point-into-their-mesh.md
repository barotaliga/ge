---
title: Receivers point into the mesh they were binned from; keep the mesh beside them
date: 2026-11-05
scope: engine/detail/receivers.hpp, games/shooters_edge/client/world_fx.cpp
concepts: [receivers, bin_receivers, dangling, decals, place_decal, lifetime]
---

`bin_receivers(mesh)` stores `&mesh`: a Receivers built from a local TriMesh and returned crashed
the first `place_decal` (a bus error, no stack, in a release build; the fixture lab under lldb hung).
Keep the mesh and its receivers together on the heap (Shooter's Edge's `MarkFaces`).
