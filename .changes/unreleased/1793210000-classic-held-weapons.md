---
type: Added
title: The classic pack's held weapons as view models
---

The classic-enhance plugin turns the pack's first-person weapons into the engine's view models: each read by the classic model reader, held at the eye as the classic game holds it, its fullbright skin texels glowing, and lit by the lightmap under the player (never darker than the classic floor), a shot's flash and, in the enhanced mode, the map's torches as live lights. The `classic-weapons` lab draws them over a map in first person; on a player's own pack its pictures stay on their machine. `viewmodel::resolve_over` lays a held weapon over a picture another raster drew.
