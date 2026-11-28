---
title: A baked data table links wherever its lookup is called, so keep the lookup with its users
date: 2026-11-14
scope: plugins/mountain/mapgen/generate.cpp
concepts: [real-face, real-hints, link-size, web-viewer, wasm, gc-sections, lay-real-face]
---

`generate_level` called `real_face(id)` and `real_hints(id)` itself, so every program that made a
map linked all 115 baked faces and their hints, though only the real-maps lab ever laid one: 2.9 MB
of Fall Line's 7.7 MB web viewer. The linker drops what nothing calls (wasm-ld collects unused
functions and data), so the fix is a seam, not a new library: the caller decodes the face
(`lay_real_face(opts, id)` into `GenerateOptions::real`), the level keeps the hints
(`Level::hints`) for the tarn. To find such weight, read the wasm's DATA section in 64 KB windows,
compress each, and grep the source tree for a dense window's bytes.
