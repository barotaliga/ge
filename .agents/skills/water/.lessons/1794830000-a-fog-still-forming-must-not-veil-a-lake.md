---
title: A fog still forming must not be drawn at a kilometre's range, and an open lake in winter always steams
date: 2026-10-31
scope: engine/wind/mist.cpp, engine/render/mist_fog.hpp, plugins/mountain/app/mist_look.hpp, games/fall_line/labs/gpu_lake_lab.cpp
concepts: [fog, mist, visibility, kformed, steam-fog, radiation-fog, lake, dawn, wind]
---

`mist_at` used to give every fog over `kNoMist` (0.02) at least its kind's clear range (1 to 2 km). Drawn as a volume, that is 0.003/m, and across a 180 m tarn it veiled 40 % of the view. So a windy dawn (strength 0.06) fogged the lake almost as visibly as a still one. Now the range eases in the logarithm from the clear air's 20 km to the kind's below `kFormed` (0.3). A formed fog keeps its bits; the golden's weak rows moved. Before drawing any `Mist`, check what its density does over the distance the eye sees through.

Fall Line's day is a winter's (about -12 C on seed 38). Any open water at 0 C and above is 6 K and more warmer than that air, so it STEAMS in any wind (sea smoke). That is the physics, not a bug. A "windy dawn does not fog the lake" picture needs a day whose dawn is near the water's temperature (the `lake` lab's `--spring`, a mean of 3 C, with the tarn at 4 C). A real winter lake waits on lake ice (M24l) before it can show radiation fog.
