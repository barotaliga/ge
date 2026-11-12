---
title: Copy a recorded move whole, prove its keys by the move first, and play its view as it is
date: 2026-11-12
scope: plugins/shooter/recipes.cpp, plugins/shooter/pilot_recipes.cpp, plugins/shooter/bot.cpp, games/shooters_edge/labs/se_recipes_lab.cpp, games/shooters_edge/bots/hop_start_book.cpp
concepts: [recipe, recipes, hop-start, copy-movement, play_recipe, proof, keys-read, view-writer, look, mouse-hand, keyboard, book, se-recipes, ground-strafe, time-to-400]
---

- Measure people and bots by ONE rule before copying: `se-recipes` on people's folder and on
  `se-team --save` matches. The rule that showed the gap was the simplest (every stop: the time
  to 400 however it is done): people 35 % within 2.5 s, bots 6-9 %; the chain-based hop-start
  count first read 11 a player-minute because single jumps onto lifts and ledges counted.
- An aggregate across a turn's side washes a curve out: the side must be the ground run's own
  (the heading's turn before the first jump), not the whole start's with its air weave. One
  start printed raw showed the 400°/s ground circle the table had hidden.
- People build 400+ ON THE GROUND (the reach before the first jump in 432 of 454 stand starts):
  check a surprising reading against the move's own rules before calling it a bug.
- Prove the reader before building on it: the recorded keys and view played through
  `player_move` on an open floor took off at 434 against the recorded 441. That one table
  settled that copying whole works.
- In a match, layers above the pilot rewrote the copy: a watch (`look`) held the view and turned
  the keys under it, the mouse hand chased a 400°/s sweep in strokes. The recorded view is
  already a person's mouse: played as it is, the takeoff went 341 -> 416. Trace `view` first
  when a staged move works and the match's does not.
- Filter the book for what a bot can use: people's "made" starts include backing off first
  (S and W share a finger: late through every change) and loops away and back (1.45 s, ending
  184 units from where they began).
