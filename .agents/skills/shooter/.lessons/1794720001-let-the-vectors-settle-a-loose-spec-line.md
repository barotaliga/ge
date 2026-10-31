---
title: When a step disagrees with its vectors, work the numbers and send the gap to the spec
date: 2026-10-31
scope: plugins/shooter/framemove.cpp, plugins/shooter/hull.cpp
concepts: [framemove, vectors, spec-gap, swim, hull-trace, clean-room, clip]
---

The original move's first run over its 35 recorded scenarios had three failures. Each was
settled by working one failing step's numbers by hand from the line before it:

- `clip` is the step-up slide's blocked bits (after any unstick) + 256, not the first slide's.
  The 258, 263 and 256 values in the vectors give this away.
- The swim's room to accelerate is the wish speed minus the speed left after the water friction,
  not the ground rule's wish − v·d. One step (swim_up_and_jump 83) separates the two readings.
- A trace into a slope-and-wall corner ended 1/32 short of ours. This was a gap in the trace's
  note, which was wrong for client and server alike: every straddled split is backed off.

Do not guess the original's behaviour from memory. Ask the spec thread to fix the note and
record vectors that isolate the case (it added `vectors/hulltrace.txt`), and build only once
that fix is on main. A scenario that passes in one recording set (pmove.txt) can still hide a
rule the other set exposes, so hold every module to every set that reaches it.
