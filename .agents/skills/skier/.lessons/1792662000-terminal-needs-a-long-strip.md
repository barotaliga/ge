---
title: A schuss reaches its terminal speed only after some 2.5 km, so the drag strip is 4 km long
date: 2026-10-21
scope: plugins/skier/synthetic.cpp, plugins/skier/scenarios.cpp
concepts: [terminal-speed, schuss, drag-strip, top-speed, scenario-length]
---

The pull down a 20 degree groomer less the air's drag falls off as v^2 nears the terminal, so a
tuck from rest is at 94 km/h after 50 s and 1.1 km but needs about 90 s and 2.5 km to come
within a tenth of a km/h of its 117 km/h. A schuss scenario that stops short reports a top
speed under the terminal and reads as a model bug when it is a short run. `flat_strip` is
4096 m for that reason, and the schusses run 90 s. A gap that stays the same however long the
strip is IS a model bug: the first one found was the groomer's pressed centimetre being charged
the powder's plough (`powder_drag` now charges the loose share alone).
