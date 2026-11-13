---
type: Added
title: The lightning beam burns a glowing line and melts the wall where it stays
---

A held beam now traces a burnt line across a wall as it sweeps (engine/detail's `burn_line` decal, laid in overlapping pieces by `render::beam_burns`) instead of a row of round scorches, white-hot where it has just been and cooling to black; held on one spot it melts a glowing pool that grows while the beam stays (the `melt` decal, pressed again larger in its own slot with `DecalAt::slot`). Glowing marks light the room while they glow (`render::mark_glows`), and `render::press_glow` presses the same glowing mark for any hot hit. `DecalAt` gains `length_m`, `along`, `slot`, `serial` and `ember_s`; `BeamLook`'s `scorch_m` and `scorch_every_s` knobs are replaced by the line's and the melt's.
