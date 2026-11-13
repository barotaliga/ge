---
title: A figure in many skins is one model and skin-only models, not a model per skin
date: 2026-11-13
scope: engine/render/morph.hpp
concepts: [morph, skin, atlas, player-skins, skin-only, morph-draw-skin, wear]
---

Drawing each player in their own picture tempted a copy of the figure per skin, which repeats
every frame of every vertex in the storage buffer. The morph pass already keeps a skin's place in
the atlas per model, so a skin is uploaded as a model with a skin and no frames (no vertices, no
triangles; it costs only its atlas rectangle) and a draw names it in `MorphDraw::skin`: the vertex
stage reads the figure's model, the fragment stage reads the rectangle of the model it wears. The
skins are read with the map's models (the atlas is built once), each kept only while
`pack_morph_atlas` still fits the lot, so enhanced or stand-in skins already in the atlas are
never pushed out by players' pictures.
