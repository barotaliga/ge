---
title: Quake II's server fires a frame and steps past it in one rules frame; a client never sees a first-frame shot on its frame
date: 2026-11-23
scope: games/shooters_edge/client
concepts: [held-weapon, firing-frames, q2, super-shotgun, shotgun, step-q2-held, spent-shells, ce-shells]
---

The Quake II hand's step (`q2_combat.cpp`) starts a run on the first fire frame, fires it and
moves to the next frame in the same step. A client reading the hand's frame ten times a second
therefore never sees the shotguns' fire frame (their first), so a check of "the hand is on a fire
frame" missed every shotgun shot: no muzzle fire, no kick, no spent hulls, while the machine gun
(a loop inside its range) looked fine. Count the frames PASSED since the last step (from the
first frame of the run when it starts, a loop back to the range's start as a shot), and keep the
hand's own weapon and state for it: `recorded_weapon` is set only when the models are loaded, so a
test without models saw a "new" run every step. Found by a lab that counted the shells a shot
threw (`super-shotgun-shells` gave 0) rather than by its picture.
