---
type: Added
title: The classic shareware pack enhanced on the player's machine
---
`plugins/classicenhance` finds the player's own copy of the classic shareware pack (the unchanged release zip, whose self-extracting LHA installer it unpacks, an installed folder, or the pack file), refuses anything whose SHA-256 is not a known unchanged pack, and enhances it into a local cache: every wall texture upscaled along its edges with generated height, normal, roughness and emissive maps, tiling kept, and each map's lightmaps packed for the rebake to replace. Nothing read from the pack or made from it is committed or shipped. Run it with `ge tool classicenhance -- verify|run|status`; `ge tool <plugin>` builds and runs any plugin's `tool/` executable. The engine gains zip archives (`vfs::read_zip`, stored and deflated, CRC-checked), `assets::sha256` and `assets::inflate_raw`. The `enhance` lab draws painted fixtures before and after.
