---
title: The nav grid misses a floor narrower than its offset, a climb under an overhang, a blade's button
date: 2026-11-28
scope: plugins/shooterbots/nav_columns.cpp, plugins/shooterbots/nav.cpp, plugins/shooterbots/map_nav.cpp, plugins/shooterbots/buttons.cpp, plugins/shooterbots/way_through.cpp
concepts: [nav-grid, column-floors, pocket, escape-pod, ladder, overhang, func-rotating, blade, boss2]
---

- The grid's columns stand 32 apart at fixed offsets; a floor between walls narrower than the box
  plus the column's offset (boss2's escape pods, 44 across with columns 16 off the middle) gets no
  node at all, and the exit inside it reads as unreachable. Slide the column's box 8 or 16 each
  way before giving up on the space (`column_floors`' pocket), but keep only a slot: a wall again
  within 32 past the slide, and no neighbour column's floor open to it. A looser pocket added 150
  stray nodes on base1 (sills, trims), cost base1 and e1m4 their exits, and moved four bot tests;
  and the scan must go on down the column as before after a pocket. Count the pockets on a map
  (a temporary print) before trusting a campaign run; test with brushes:
  `a_slot_narrower_than_the_grids_offset_gets_a_node_on_its_floor`.
- A ladder brush is linked foot to top by its box alone; a ladder that runs on below an overhang
  holds the climber's head under it for good. Sweep the player's box up the face before linking
  (`ladder_climb_clear`).
- A button whose relays only start a `func_rotating` opens nothing and can shut the way behind the
  bot (boss2's gallery blade): such a button is no step (`button_spins`). A floor plate starts it
  too when walked over, so its sweep is harm (`blade_sweep`); a brush model turned round an
  `origin` has its box built round that origin, off it: move it there first.
- Check a moved map against today's main before calling it a regression: of eight maps worse than
  a day-old baseline, six moved the same on main.
- `brush slice` shows only solid and liquid; a bevel or a slope the box lands on needs `se-spot`'s
  cut pictures at the place.
