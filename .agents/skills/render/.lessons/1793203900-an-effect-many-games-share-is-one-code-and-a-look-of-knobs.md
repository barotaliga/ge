---
title: An effect many games share is one code and a look of named knobs, read per instance
date: 2026-10-28
scope: engine/render/explosion.hpp, engine/render/explosions.wgsl
concepts: [explosion, knobs, preset, look, per-instance, gpu-timestamps, llvmpipe]
---

A fireball first built as a table of three kinds with the shape and the smoke's colour as shader
constants had to be reworked the same day: weapons and games want the same blast with different
fire, smoke and sparks. Build such an effect as one code over a struct of named knobs (each with a
unit, a line of what it does and a clamped range, set by name) with presets as starting points,
and put every knob the shader needs in the per-instance record (two more vec4s here), so mixing
looks in one frame costs nothing. A sweep in the lab that shows each knob at its two ends is the
quickest proof each one does what its name says. And time a GPU pass only with nothing else
running: on llvmpipe a parallel build or a second lab turns the pass's timestamps into noise
(one blast read as 0.07 ms, the timer's floor, and a filled screen as 7 ms).
