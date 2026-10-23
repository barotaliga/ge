---
title: A pilot holding its height over the snow must slow for a rise it cannot climb at speed
date: 2026-10-22
scope: plugins/helicopter/pilot.cpp, plugins/helicopter/spec.hpp
concepts: [pilot, terrain-following, climb-rate, look-ahead, descent, speed-cap, outing]
---

On the free ride the bot flew from the valley pad toward the highest run's head at the 40 m/s
cruise and met a crown on the face every time. Its climb is capped (`pilot::kClimb`, 8 m/s), and
40 m/s up a face steeper than one in five rises faster than that. Holding the height over the
snow ahead only helps if the machine can reach that height in time.

The fix plans the speed off the way ahead: every point read within `kLook` m is reached no
sooner than the climb to clear it takes (`kClimbShare` of the climb, the rest kept in hand). The
same rule runs the other way home: a machine 1,000 m over its pad slows so its sink can bring
it down by the time it arrives, but never under `kDescend` (18 m/s). Below that it would be held
to the hover's slow sink of 2.5 m/s, and the descent would take minutes.

A scenario on the hand-built slope never showed this: its face is short. Test a pilot over a
whole mountain (the `free` lab's `--heli` outing on seed 38) before trusting it.
