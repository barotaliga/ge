---
title: Under the keyboard a pressed jump lands late; read the command row by row before judging a jump's proof
date: 2026-11-14
scope: plugins/shooter/pilot.cpp, games/shooters_edge/bots/fighter.cpp, games/shooters_edge/bots/keyboard.cpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/se_reach_lab.cpp
concepts: [rocket-jump, flip, liftoff, shotjump, rjready, keyboard, jump-key, landed-short, missed-jump, rocket-ready, keys-by, south-pit, dm2, lava, rockets-fly]
---

- dm2's south-pit flips "landed short" though the nav proved them (staged analog: 15 of 15). The
  match's rows told why at once: health 100 -> 51 and 330 -> 667 u/s on the ground, jump 0 on the
  shot's command and 1 three commands later. The shot leaves with the ask; the keyboard's jump key
  goes through the hand's latency. Any pair of actions a person presses as one move (fire and
  jump) must wait for the slower path: hold the fast one until the slow one is down (`shotjump`).
- A pilot that reads "landed" on the first command on the ground after its jump ask misreads
  every late jump as a miss (`missed_jump`, twice: never planned again) and walks the flight it
  then takes. Give a lift-off a grace (`liftoff`, 0.15 s) wherever an ask goes through a hand.
- An action needing a tool must check the tool in hand AT the action: a third of the rocket jumps
  asked were fired with another weapon (no blast), because the weapon switch ahead of the link is
  a request, not a guarantee (`rjready`).
- When a fix to one piece of a command moves nothing, find WHICH piece wrote the keys: se-team's
  `keys` column (fighter.hpp `keys_by`, marked after each writer by comparing the keys) named the
  "no route" idle as a tug of war between `lost` and `brink` in one set.
- A staging that disagrees with the match is the staging's bug first: se-reach `--rockets=fly`
  under `keys` never fired a flip the match's fighter fires; analog (`--gap-features=-keys`) it
  matched the proof. Say which switches a staging can be trusted under.
- The proof it was the misread: with `liftoff` on, honouring every jump missed twice (`jumpcuts`)
  took dm2's jumps 125.7 -> 113.3, not the half it took before. A rule that "broke" the bot may
  have been reading a broken signal: fix the signal, then judge the rule again.
