---
title: A glyph mask is drawn at its keyed size, or a lab's pixels depend on the lab before it
date: 2026-10-31
scope: engine/ui/font.cpp
concepts: [font, glyph-cache, size-key, determinism, digest, labs-smoke, shard]
---

`Font::mask` keys its cache by the size rounded to 1/16 px. It used to draw the mask at the
caller's exact size, so two sizes within 1/32 px shared whichever mask was drawn first. One lab
alone gave one digest, and the same lab after another lab in the same `--all --shard` process
gave another, because the built-in font is shared. The fix is to draw at `size_key / 16`. A
digest that passes when the lab runs alone but fails in a CTest shard, after a new lab shifted
the shards, points at shared cache state like this one. Reproduce it with the shard's own
command, `labs --all --smoke --shard=i/N`.
