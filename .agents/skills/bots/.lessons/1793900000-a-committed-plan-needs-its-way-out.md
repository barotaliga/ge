---
title: A plan that commits a bot past its other rules needs its own way out, measured in time spent
date: 2026-11-01
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [dive, commit, give-up, stall, air, swim-field, rng, prng, dive-wet-s]
---

The dive was committed once in the water (no other goal, no air rule) so a swim was not turned
round half-way, and nothing ended a dive that could not finish: on e1m2 the rocket launcher's
dive fails at its ledge, and with no drowning to show for it the bots simply bobbed there, blue2
312 s and red3 220 s of a ten-minute match (716 s over the eight). A metric of TIME spent in the
committed state (`dive s` in `se-team`'s legs table) found it; counts of dives begun did not.
The way out is a rule on the plan's own clock (in the water longer than the swim's planned time
plus 3 s, or short of air with the swim field's way back shorter than the dive's way on), then a
cool-down on that item so the next think does not pick the same dive again: 64 s wet after.

To measure a fix whose "before" lacks the counter, run a CONTROL with the counter on and the fix
switched off (`if (false && ...)` for one build, restored after), not the old tree: anything else
in the change (here the explorer's random stream) moves the match, and the control isolates it.

Every draw comes from `core::Rng` (`make_rng`, `pick_index`), a bot's own included; no check
catches a hand-written generator in game code, so a review has to.

`make precheck` and `ge digest --affected` both passed this change while `se-team`'s smoke digest
had moved (a new table column): after touching a lab's tables or metrics, run `ge digest <lab>`
by name before the push.
