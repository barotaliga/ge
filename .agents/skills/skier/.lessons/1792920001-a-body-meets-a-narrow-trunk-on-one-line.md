---
title: A trunk is narrower than the skier, so only one line of his front meets it; a whole-front blow kills at 43 km/h
date: 2026-10-23
scope: plugins/skier/body.cpp, plugins/skier/anatomy.hpp
concepts: [body, injuries, trunk, tree, front, iss, severity, landing, kicker, risk]
---

The first cut of the body carried him on into a trunk square ahead of him with his whole front:
the chest, belly, pelvis, head, both shoulders, both arms, thighs, knees and shins at the
closing speed, against the trunk's 1 cm of give. Every scenario that met a trunk was then an
ISS of 75, dead, even at 43 km/h, which is the mean speed of real tree collisions. Their mean
ISS is about 15. A trunk 40 cm across meets one line of him, so now it is one shoulder in full,
the chest at the trunk's share, the head in its helmet glancing, and the hip under it at half.
After that change the `injuries` lab's head-on curve gives ISS 31 and one death in eight at
43 km/h, rising to certain death past 65.

Read a crash model off the sweep over draws (`injuries` lab, `trunk` and `landing` tables),
never one scenario: one run is one draw of the risk curve. The synthetic kicker lands at 7.7 g
and its pop at 11.4, past a clean landing's 6, so a strained back there is a real chance. A test
of "a clean ride hurts nothing" must use rides that land under 6 g.
