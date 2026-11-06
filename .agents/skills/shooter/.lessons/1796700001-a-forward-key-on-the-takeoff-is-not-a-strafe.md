---
title: A switch that acts on the forward key alone also acts on a strafer's takeoff command
date: 2026-11-06
scope: plugins/shooter/pmove.cpp, tests/shooter_pmove_test.cpp
concepts: [speedjump, easy-jump, strafe, takeoff, bit-identical, scripted-hands]
---

The easy jump (`pm_speedjump`) acts when the forward key is held without a side key. Scripted
strafe hands that press forward (and no side key) on the command they jump from get the easy
gain on that one command, so a "strafe is untouched" check fails by a few ulps a hop. Real
strafers hold the side key through the takeoff; script them that way, and keep the bit-identity
check: it is what proves the switch never reaches a strafe.
