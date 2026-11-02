---
title: A route along a pool's floor drowns bots, and a working pickup can hide a dive that fails
date: 2026-11-01
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/knowledge.cpp
concepts: [drowning, air, dive, swim-field, item-knowledge, timing, obituaries, mvd]
---

Count deaths by cause before tuning the aim: the recording's obituary prints (`strings` over the
.mvd, names folded) showed 13 of 90 deaths on e1m2 were drowning, from routes the nav graph walks
along a pool's floor. Turning a swimmer up and out after six seconds under removed them and
lifted the frag rate more than any aim change would have.

The same change showed that the rocket launcher had been reached by those floor walks, not by
the dive built for it: the dive's last step, from the surface onto the pocket's ledge, fails
(the swimmer bobs at waist depth below a ledge too high to step and too low for the move's water
jump). Trace one swimmer's position, waterlevel and way every few commands before trusting a
metric that only counts the item held.

Item beliefs from sight change behaviour more than the timing rule suggests: gating every item
believed gone and untimed out of the goal list stopped teams from waiting at the launcher at all.
A guessed (late) time back keeps that waiting for players who do not time, which is how an
average player behaves.
