---
type: Added
title: A classic release known by its files
---

classic-enhance now recognises the full game whatever packs hold it: its two packs as shipped, repackaged into one `pak0.pak`, or a content zip `pak0.pk3`, extra files ignored, every one of the release's files checked by path, size and digest (`Source::set`, `known_sets.hpp`); the 2021 remaster is known the same way and keeps its own remade sounds, its textures and models enhanced like the original's; `ge tool classicenhance -- manifest` prints a source's files as a known set's rows.
