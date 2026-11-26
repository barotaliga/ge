---
title: A weapon swapped in steps its own game's hand on a clock that adds its step
date: 2026-11-25
scope: games/shooters_edge/rules/common_q2_arms.cpp, plugins/classic/q2_arms.hpp, tests/shooters_edge_remix_q2_arms_test.cpp
concepts: [remix, cr-items, q2-arms, hand-step, refire, period, weaponframe, server-frame]
---

Quake II's weapons in the classic rules keep their period because the rules step Quake II's hand
at its own ten steps a second by a next-step time that adds 0.1 each step (`f_q2_next`), not by
`time + 0.1` from whichever frame ran it, which would round each step up to the next server
frame. A row's refire then only describes the weapon. Measure a period in a test by the server's
own time (`sv.time`), never by counting frames at an assumed rate: the listen server does not run
72 frames a second, and a count of frames read the shotgun's 1.2 s as 1.28 s. Keep the hand's
frame in the player's `weaponframe`, so the window and a joined client draw the view model on it
with every frame kept.
