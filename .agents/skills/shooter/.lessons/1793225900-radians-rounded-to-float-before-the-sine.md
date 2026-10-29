---
title: The view's radians are rounded to float before the sine; a sideways drift of 1e-7 u/s says so
date: 2026-10-28
scope: plugins/shooter/vec.cpp
concepts: [angle-vectors, float32, radians, sin, parity, recording, drift]
---

Taking the sine of the angle in degrees times 2 pi / 360 straight in double gave sin(180 deg) =
1.2e-16, and a player facing -x moved sideways by nothing. The recordings show -2.6e-6 u/s
sideways at 30 u/s: the sine of pi rounded to float (-8.74e-8). So the radians are rounded to
float first, then the sine taken in double and rounded. A difference of 1e-6 on an axis the
player never steers along is the first sign of a precision step taken in the wrong type: read
the recording's tiny components, not only the big ones.
