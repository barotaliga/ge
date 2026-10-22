---
title: An occlusion cull is proved by the picture without it, to the bit
date: 2026-10-22
scope: engine/render/instances.hpp
concepts: [cull, occlusion, hi-z, two-phase, check, picture, exact]
---

A cull that drops something visible shows as a few missing pixels, which no reviewer sees in a
screenshot. The `world` lab redraws every view with `FrameOptions::occlusion` off (frustum and
size still on) and compares the RGB bytes: any differing pixel is a `cull-faults` failure. It
can be exact because the culled frame and the unculled one draw the same surfaces at the same
depths; only the draw lists differ, and the prepass plus the `equal` colour pass make the
picture independent of the order. Keep both halves of that: a draw that blends or a colour pass
that does not test `equal` would make the check order-dependent and force a tolerance.
