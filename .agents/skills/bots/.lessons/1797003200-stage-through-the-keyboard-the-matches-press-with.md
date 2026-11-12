---
title: Stage a move through the keyboard the matches press with; a jump the keyboard presses late must land when it goes down late
date: 2026-11-12
scope: games/shooters_edge/bots/keyboard.cpp, games/shooters_edge/bots/pilot_run.cpp, games/shooters_edge/labs/se_reach_lab.cpp, plugins/shooter/pilot.cpp, plugins/shooter/pilot_gaps.cpp
concepts: [keys, keyboard, curvekeys, press_keys, se-reach, gaps-fly, key-chatter, jump-key-late, gap-run, underway, fallen-under-the-way]
---

- A staged lab that presses the pilot's analog axes straight into the move judges a different
  bot from the one in matches, where `keys` turns every command into key sets through a hand.
  `se-reach --gaps=fly` staged 137 landings of 140 jumps; matches landed 45 %. Pressed through
  the same keyboard (`run_route(..., keys, moves)`, `press_keys`, on whenever `keys` is in
  `--gap-features`), the staging reproduced the match at once: 101 jumps, 81 landed. Compare a
  staged run with `-keys` to see what the keyboard costs before reading matches.
- A curve the pilot means as W and D is a slewed side axis after its hands, turned under a mouse
  view a few degrees behind; the keyboard read off that taps D in and out (`keysteer`). Hand the
  keyboard the keys the pilot MEANS (`Pilot::curve_keys`, held under `curvekeys` while the
  command is still the pilot's: a hop, the view within 20 degrees, the brink guard not in it).
- The keyboard's jump key goes down one or two commands after it is asked. On a curve that late
  press leaves on a line a few degrees round: it clipped a wall a gap away. Ask for the jump only
  where it lands across asked now AND down one and two commands later on the keys held
  (`gap_lands_late`; in the rollout, two places in a row land), and hold the keys and the view as
  asked until the feet leave the ground (a run re-planned in between swung the view 24 degrees
  at the takeoff). Landed share 58 % -> 93 % staged.
- Read the trace for runs that never use the trick too: a keyed hop overran a ledge (faster than
  the analog one) and then circled under a node 144 units up for 2.5 s until the stuck check,
  which compares distance ACROSS only. A walker on the ground under both ends of its link has
  fallen under the way: plan again at once (`underway`), as a swimmer under it already did.
