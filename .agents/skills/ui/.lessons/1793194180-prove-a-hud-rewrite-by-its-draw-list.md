---
title: Prove a HUD rewritten as data by its draw commands, not by the hud lab's digest
date: 2026-10-28
scope: games/fall_line/hud/view.cpp
concepts: [hud, layout, parity, draw-list, digest, float, stack]
---

The `hud` lab's pictures are measured, so its digest holds only the snapshot's figures and stays
the same whatever the HUD draws. To show that Fall Line's HUD re-declared as elements draws what
the hand-placed view drew, keep the old view in a scratch test and compare the two `DrawList`s
command by command (each rect and baseline compared as bits) over many snapshots and screen sizes.
Bit equality also decides how offsets are added: `place(..., dy)` and a stack built with that
`dy` give the same float, but `bottom() + gap` does not equal `s.y + (used + gap)`, so a
clearance rule that must keep old pixels is written in the stack's own terms (its `used`).
