---
title: An inertialized camera switch keeps every component of the velocity
date: 2026-10-23
scope: engine/camera/transition.cpp, engine/lab/labs/camera_transitions_lab.cpp
concepts: [camera-transitions, inertialize, decay, velocity, kick]
---

The animation form of inertialization decays the offset as one vector and keeps only the
velocity along it, dropping one that moves away. For a lens that orbits a turning subject most
of the velocity is across the offset, so the switch kicked the eye by up to 10 m/s. Decay each
axis of the subject's frame on its own, keep the velocity whichever way it points, and keep the
initial pull clamped at 0 (an unclamped one is its own kick). Measure a switch by the change of
the eye's velocity in the subject's frame (`switch-kick`), not its displacement: a lagging rig
moves fast in that frame on its own.
