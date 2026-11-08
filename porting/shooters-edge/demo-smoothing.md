# Recorded games drawn between their frames

**A WORKING NOTE** (README.md's rules). Where the facts come from: a research session (2026-11-08)
that read today's community client's demo playback and the modern server's multiview recorder, and
wrote only these facts in our words. The building session reads this note and the measurements, not
the source.

## Why a played recording looks jerky without it

- The modern server writes a multiview frame at most `sv_demofps` times a second (default 77; 0 means
  20); with no players moving it falls to `sv_demoIdlefps` (4..30). A frame's players are where the
  server last moved them, which is where each player's own last command left him: a player whose
  packets arrive unevenly moves unevenly from frame to frame.
- A picture drawn at a rate that is not the recording's (a 60 Hz display, a 30 fps video) shows each
  frame for a different number of pictures, so even even motion judders. The client hides both by
  drawing the world BETWEEN the last two frames it read, never at the newest one alone.

## What the client does (multiview playback)

- **One frame behind.** It reads a frame once the playback clock passes the time the frame before
  it carried, so it always holds two: the older (`from`, time t0) and the newer (`to`, time t1),
  with the clock between them. A picture is drawn at `f = (clock - t0) / (t1 - t0)` held to 0..1.
  A recording's clock is the sum of the records' milliseconds bytes.
- **Players**: the origin is `from + f * (to - from)` on each axis; the view angles turn the
  shortest way round (a difference of 180 degrees or more is taken the other way round the circle).
  A player is drawn at `to` (not between) when:
  - he is missing from either frame;
  - he moved more than 150 units between them (a teleport, a respawn's jump);
  - he was a corpse in `from` and is alive in `to` (a respawn);
  - he is gibbed in `to` and was not a corpse in `from` (the first gibbed frame).
  - The angles alone are not turned between frames when the server set that player's view in `to`
    (the multiview `setangle`, which names the player): they jump.
- **The tracked player** is drawn the same way: the eye rides his in-between origin and his
  in-between view angles, so the camera is as smooth as the bodies.
- **Other entities** (rockets, grenades, nails, doors, lifts): drawn between their origin and angles
  in the last two frames on the same rule, unless the model changed or they moved more than 200
  units (then drawn at `to`). Rotating pickups spin on the client's own clock, not the frames'.
- **The models' animation frames** are blended too, separately: a frame change is blended over a
  tenth of a second from when it was seen. (Not taken here yet: the engine draws one frame.)
- With interpolation the picture is up to one recording frame (13 ms at 77 Hz) behind the newest
  data; nothing else is predicted from velocities in recorded playback.

## Taken here

`games/shooters_edge/client/demo_lerp.hpp` draws every format (single, client, multiview) between
its last two frames by the rules above, in the window's `playdemo` (`demo_lerp 1`, the default)
and in `-demovideo`; `timedemo` (the benchmark) draws each frame as read. A gap between frames longer
than a quarter of a second (a stall, a pause) is not drawn across: the picture shows `to` as read.
