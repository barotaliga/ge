---
type: Added
title: Shooter's Edge's picture quality from 0 to 10
---

`r_quality 0` to `r_quality 10` sets Shooter's Edge's whole picture at once: 0 is the classic look, 9 the picture as it ships, 10 every enhancement (every map light throwing the figures' shadows too). Each level down gives up the dearest thing left, so it draws faster: the shadows' soft edges, the mirrored water and the far land, the contact shade, the cloud and the reflections, then shadows, water and sky, and at the bottom the picture drawn at half and a quarter of the window's size. A level only writes the settings (each `ce_` switch, its level, `r_look`, `r_scale`, ...), so any of them can be changed after it; `r_quality` alone says which level the settings make, or that they are your own mix. The VIDEO page shows the same as QUALITY, or CUSTOM. The engine's `console::PresetTable` and `add_preset_command` make such a command for any game's group of settings. The quality ladder's `r_quality low|medium|high|top|auto` and `r_budget` are gone from Shooter's Edge.
