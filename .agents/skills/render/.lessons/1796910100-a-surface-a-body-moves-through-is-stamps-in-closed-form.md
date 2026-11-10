---
title: A surface a body moves through is a few dozen closed-form stamps, with a CPU twin for the lab
date: 2026-11-10
scope: engine/render/liquid_wade.hpp, engine/render/wgsl/liquid_wade.wgsl, games/shooters_edge/client/world_extras.cpp
concepts: [wade, wake, furrow, rings, viscosity, stamps, cpu-twin, uniform, closed-form]
---

To make a liquid react to a body in it, no height-field simulation is needed: stamp the body's
track (a stamp each spacing moved, its velocity and the span to the one before, the newest a
"head" drawn on ahead along its velocity) into the body's uniform, at most 48, and evaluate each
stamp in closed form per pixel: the furrow as a capsule to the stamp before (the nearest point's
time gives its age), the rings as a packet out of the stamp. Take the MIN of the troughs and the MAX
of the lips over stamps, not their sum, or every joint between two segments digs twice. Keep the
shader's function and a CPU twin (`wade_height`, on the vendored libm so its pictures are the same
bits everywhere) side by side: the test holds the twin's shapes, and the lab's CPU half draws the
surfaces from above without a GPU, which is the picture that tunes the constants. The slope comes
from the height's screen derivatives (as lava's crust does), so a slime's analytic ripple slope and
the wade's sum without a second evaluation.
