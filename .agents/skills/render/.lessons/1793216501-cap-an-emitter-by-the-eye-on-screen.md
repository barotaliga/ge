---
title: Cap an emissive line's screen width by the eye, or its root fills the view with white
date: 2026-10-28
scope: engine/render/beams.wgsl, engine/render/beam.cpp
concepts: [beam, emissive, screen-space, bloom, first-person, near-camera, forks]
---

A beam fired from a first-person gun starts half a metre from the eye, so a core a few
centimetres wide covers dozens of pixels there; at its brightness the bloom turned it into a
white blot over a quarter of the view. Cap both the sheath (a thirtieth of the screen's height)
and the core (a hundredth) in pixels, keeping brightness rather than energy past the cap, and
keep side detail (forks) out of the first 1.5 m. A helix drawn by a walk needs steps short
enough for its turns (three steps a quarter turn), or it reads as crossed sticks; a smaller
step also means more forks a metre, so scale the fork chance with it.
