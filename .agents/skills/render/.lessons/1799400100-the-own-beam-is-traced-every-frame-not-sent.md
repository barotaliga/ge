---
title: The own lightning beam is traced every frame; the server's end jumps at the fire rate
date: 2026-11-23
scope: games/shooters_edge/client/own_beam.cpp
concepts: [lightning, beam, fakeshaft, burn-line, marks, aim, own-beam, chords, corner, edge]
---

A beam message comes only when the gun fires (every 0.1 s) and a round trip late, so anything keyed
on the server's end (the drawn bolt, `beam_burns`' line, the sparks) moves in steps and the burnt
line is straight chords between them. For the player's OWN beam, trace it again each frame from
the drawn eye along the view (`own_beam`, `cl_fakeshaft`) before the beams are drawn and burnt; keep
the server's line (`HeldBeam::server_from/to`) only for the share and for when there is no map.
A sweep crossing to another face restarts the line (`beam_burns`' jump rule), which left a gap
at every corner: find the edge between last frame's aim and this one by halving the turn
(`beam_edge`) and burn up to it with `finish` before the new face's point. Throw the server
message's sparks at the traced end, not the sent one. Picture it with
`se-weapon-fx --sweep=80,300,250 --before="cl_fakeshaft 0"`.
