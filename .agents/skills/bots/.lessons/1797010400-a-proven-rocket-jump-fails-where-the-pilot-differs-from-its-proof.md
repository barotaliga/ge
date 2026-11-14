---
title: A rocket jump or escape the nav proves fails live wherever the pilot starts, keys or reads differently from its proof
date: 2026-11-07
scope: plugins/shooter/nav.cpp, plugins/shooter/escape.cpp, plugins/shooter/pilot.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/pilot_run.cpp, games/shooters_edge/labs/se_reach_lab.cpp
concepts: [rocket-jump, flip, escape, lava, proof, run-up, turn-room, liftoff, shotjump, rjready, missed-jump, keyboard, staged-run, dm2]
---

- The flip (look back and down, fire with the jump, turn in flight) flies 45 % farther a second than
  a shot at the feet, 40 % lower. Proofs no pilot plays: a running start at a wall or with no floor
  behind (`room_to_run_up`, `turn_room`: a person's 0.4 s turn at 320); a refused proof's `end` at 0
  0 0. Put the technique outermost in the try loop, or the nearest pair picks it. Start the turn
  from the walk (`flip_ahead`), led by the view's hand (delay + 180 / slew). Rank landings; never
  refuse one that leads nowhere yet (a later round bridges on from it).
- The lava escape (`escape.hpp`): the pilot read the server's waterlevel, the proof the move's; the
  pilot pitched only on the firing command, the proof through the backing (in a liquid a pitched
  view turns the back key's wish UP); a proof pitched 88 where the hand stops at 80. Most live shots
  come from bots nearly dead (deep lava burns ~120 a second). A goal set mid-flight idled the pilot:
  a flight restores its mode on landing, unless it died.
- Under the keyboard the shot left with the ask and the jump three commands later: `shotjump` holds
  the fire until the jump is down. "Landed" on the first ground command read every late jump as a
  miss (`liftoff`, a 0.15 s grace); with it, honouring missed jumps cut dm2's jumps 10 %, not half:
  fix the signal, then judge the rule again. Check the weapon in hand AT the shot (`rjready`): a
  switch ahead is a request.
