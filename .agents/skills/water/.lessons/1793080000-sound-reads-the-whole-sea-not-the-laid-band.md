---
title: The lapping reads the whole sea (Sample::hs), not the laid band's surface
date: 2026-10-24
scope: engine/water/sound.cpp, engine/water/world.hpp
concepts: [water, sound, waves, lay, hs, lake]
---

A lake's sea under a fresh breeze has a peak period under a second, so its waves are about a
metre long and the simulation's band (`Lay::shortest` 1.5 m) lays almost none of them: the
surface `sample` returns over a 300 m lake at 10 m/s moved 4 mm rms against an Hs of 8 cm, and
the lapping came out the same in a calm and a breeze. The sound reads `Sample::hs`, the whole
spectrum's height at the point (fetch and depth applied), and lets the laid crests rise over it
as they pass. Anything else that wants "how rough is the water here" (foam, a spray rate, a
boat's slam) should read it too, never the laid surface's variance.
