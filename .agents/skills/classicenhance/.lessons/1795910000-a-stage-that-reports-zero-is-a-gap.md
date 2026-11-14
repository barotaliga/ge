---
title: A stage that reports 0 on another game's packs is a gap, not a pass
date: 2026-11-14
scope: plugins/classicenhance/pipeline.cpp
concepts: [textures-stage, wal, external-textures, sequel, material, marks, profile-words, pack-textures]
---

The textures stage ran green on the sequel's packs for days while enhancing nothing: its maps keep
their pixels in `textures/<name>.wal`, so every `brush::Texture::indexed` was empty and
`pack_textures` skipped them all, and the note said "0 textures". Run each stage on every content
row's real packs (`classicenhance run <pak0> --only=pack,<stage> --upscaler=kernel`) and read
the counts before calling a row covered.

Such a family's maps list every brush side's texture (clip, trigger, hint, the sky's
placeholder), so take only textures a surface is drawn with, and leave sky-marked ones out (the
sky box is drawn there). A texture's material comes from the reader's marks (`sky`, `liquid`)
and the row's `Profile::words`; the name rule ('*', "sky") is only `marks_by_name`, for a picture
known by name alone. The window needs no change: it finds the cache by the packs' key and the
textures by name.
