---
type: Added
title: The engine's mover carries the classic arena feel over brush worlds
---

`plugins/shooter` now runs the classic player move on the engine's own mover and swimmer: `classic_rows` builds `engine/move` and `engine/water/swim` rows from the server's settings, `row_command` runs one command through them, and `hull_world.hpp` answers the mover's trace with a brush model's clip hulls and lays its water, slime and lava as `engine/water` volumes. Every line of 28 recorded scenarios lands within the recordings' own tolerance. To make this possible, `engine/move` gains the knobs `gravitylead`, `landclipspeed`, `frictionwhole` and `clipfromstart` and the call `carry_along`, and the swimmer gains `along_wish` and `feet`. Every new knob is off by default, so no existing row moves.
