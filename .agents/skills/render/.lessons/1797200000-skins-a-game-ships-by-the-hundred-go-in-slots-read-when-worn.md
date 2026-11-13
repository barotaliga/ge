---
title: Skins a game ships by the hundred go in slots read when worn, not in the atlas at load
date: 2026-11-13
scope: engine/render/morph.hpp
concepts: [morph, skin, slots, set-morph-slot, player-figures, atlas-full, lazy-skin]
---

The second game ships about a hundred player pictures (256x256 or larger), and packing them all into
the 2048-wide morph atlas with the map's models left room for nine: every other look fell back to
the default. The atlas is built once, so it cannot grow; instead `create_morph` takes a slot count
and makes a 2D array texture beside it (`morph.slots`, 512 square a layer), `set_morph_slot` writes
one skin stretched by the nearest texel with its marks, and a draw's `MorphDraw::slot` reads that
layer over its own coordinates. The game lists the pictures when the map is read and reads one the
first time a player wears it, reusing the slot worn longest ago (never one drawn this frame).
A trap on the way: `create_models` moved the read lists into the models and then asked the moved-from
list whether it was empty to size the slots, so there were none and every figure drew in its
one-texel placeholder (white). Ask the destination after a move.
