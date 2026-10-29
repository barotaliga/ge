---
title: Dirty water under a murky preset is one flat colour; pick the look, then add the specks
date: 2026-10-29
scope: engine/render/underwater.wgsl, plugins/shooter/labs/gpu_plunge_lab.cpp
concepts: [water, underwater, murky, specks, motes, visibility, veil, lab]
---

An algal pond (0.45 m visibility) under the `murky` preset (three times the fog) drew every frame
under the surface as one flat olive card, and a brown lake 2 m down drew black: physically right,
and nothing a player reads. Dirty water reads as dirty through what is in it: specks close to the
eye (from 0.1 m, fogged over their own distance, not a fixed one) and a veil brighter looking up
than down. Pick the look for the colour, keep the preset at `eyes-open`, and let the specks'
share follow the water's attenuation. A per-pixel speck hashed in one cell is cut where its disc
crosses into a cell that pixel does not look in: keep its centre and its drift inside the cell's
middle half.
