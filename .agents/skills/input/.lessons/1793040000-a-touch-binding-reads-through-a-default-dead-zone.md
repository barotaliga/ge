---
title: A touch binding reads through the axis's default dead zone unless it says otherwise
date: 2026-10-24
scope: games/fall_line/controls/bindings.cpp
concepts: [touch, dead-zone, binding-shape, lever, stick, thumbs, exact-bindings]
---

An axis binding gets the shape's default dead zone whatever its source. That suits a stick
that rests a little off centre, but a touch control has no rest noise: its zero is where the
thumb landed. Read through the default, a lever eased to half read 0.484 and a half-pushed edge
thumb read short in the same way. Every `touch:` axis binding in Fall Line's defaults therefore
says `deadzone=0,1`, and so do the exact bindings a scripted thumbs session reads through.
Check a new touch axis binding against a finger placed at a known share of its reach, as
`the_thumbs_steer_tuck_brake_and_jump_through_the_default_bindings` does.
