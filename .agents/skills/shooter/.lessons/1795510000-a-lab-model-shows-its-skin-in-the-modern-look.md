---
title: Shoot a model's skin colours in the modern look, and place the item whose skin you changed
date: 2026-11-01
scope: games/shooters_edge/labs/gpu_se_skins_lab.cpp, games/shooters_edge/client/world_models.cpp
concepts: [replacement-skin, skin-stand-in, classic-look, palette, armour-skin, lab]
---

Two traps cost a lab rerun each. The classic look holds the frame to the pack's palette, so a
fixture pack with a grey palette turns an orange skin picture grey: shoot a full-colour skin with
`look.look = "modern"`. And an item's class picks a skin, not a model: `item_armor2` draws
`progs/armor.mdl` skin 1, so a picture for skin 0 (`armor.mdl_0`) is only seen on `item_armor1`.
A skin picture replaces one skin picture of the model (`OpenedModel::skins[i]`), counted the way
the model file lists them, not by the entity's skin group. The fixture model's texture coordinates
cover only a few texels of its 8x4 skin per face, so a cut-out pattern must repeat every few texels
of the picture to show on it.
