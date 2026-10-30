---
title: Measure the engine mover's row one command at a time; each gap was one quirk and one knob
date: 2026-10-30
scope: plugins/shooter/row.cpp, plugins/shooter/row_recording.cpp, engine/move/step.cpp
concepts: [classic-row, engine-mover, step-error, run-error, gravity-lead, clip-from-start, friction-whole, recordings]
---

Running each recorded command from the RECORDED state before it (the "step" measure) showed
every model gap as one line that stood out, with no drift hiding it. Each gap was one classic
quirk, and each closed with one engine knob that is off by default. Gravity is applied all
before the move (`gravity_lead`). A landing keeps its fall below 300 (`land_clip_speed`), and
the next ground friction counts that fall (`friction_whole`). A slide clips the velocity it
started with (`clip_from_start`). The swimmer accelerates along the wish (`along_wish`), and its
feet sample sits 1 unit up (`feet`). After those knobs, the double-precision mover agreed with
the float recordings within 1.4e-4 unit, because the hull world returns the hull trace's own
float end points. Do the hull's shift in double and round once: adding and then removing the 24
units in float moved end points by an ulp. A spectator is never categorised, so do not snap it
to the ground, and keep its water labels as they are.
