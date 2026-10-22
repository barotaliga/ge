---
type: Added
title: Instance families with two-phase Hi-Z culling
---

`render/instances.hpp` draws repeated meshes as GPU-driven instance families: packed vertices pulled from storage, up to four LODs a mesh with a dithered band between them, one indirect draw a LOD bucket, and a two-phase cull against the last frame's and this frame's depth pyramids; `render/mesh.hpp` builds and packs their meshes, and `skier/app/figure.hpp` poses a placeholder skier from a `SkierState`.
