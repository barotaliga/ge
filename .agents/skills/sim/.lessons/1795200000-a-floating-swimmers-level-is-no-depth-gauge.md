---
title: A floating swimmer's immersion level is no depth gauge; read the water's depth
date: 2026-11-01
scope: games/fall_line/sim/lake.cpp
concepts: [swim, immersion, level, wade, ashore, tarn, body-regime, buoyancy]
---

The body regime models the swimmer as an upright column, so a skier afloat on his jacket's air
rides with his waist out of the water and reads level 1 in water five metres deep. Climbing out
on `level <= 1` stood him on the bank seconds after he went in, anywhere on the lake. Decide
"he can stand here" from the water's depth at his place (the surface less the bed, `kWade`),
and keep `level` for what it means (the friction, the breath, the view's tint). Also: a test
that holds a reference into `state.level` must copy it before it rebuilds `state`, or it reads
the freed map (ASan names it).
