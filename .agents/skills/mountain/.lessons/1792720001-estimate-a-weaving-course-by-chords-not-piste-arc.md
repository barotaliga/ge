---
title: Size a stretch for a course of its own by the chords it is laid on, not the piste's arc
date: 2026-10-21
scope: plugins/mountain/mapgen/ski_cross.cpp
concepts: [ski-cross, stretch, piste, length-band, sweep]
---

The ski cross picked its stretch assuming a course weaving down it runs about 12 % longer than the
piste's arc. On a winding piste it runs much shorter, because the course's corners stand on the
piste's line a leg apart and cut its bends, so most courses came out under the 800 m band. Measure
the candidate stretch by the plan length of the chords the line will actually be laid on (a point
every leg's mean length down the piste). The swings out and the rounded corners about cancel, so no
fudge factor is needed. A course of its own over a piste is sized by its own geometry, never by
the piste's.
