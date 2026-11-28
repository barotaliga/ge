---
title: A mission pack's own mover class is air to the nav until it is named in map_nav's movers
date: 2026-11-28
scope: plugins/shooterbots/map_nav.cpp, plugins/shooterbots/way_through.cpp
concepts: [new-plat, train2, mission-pack, movers, plat, dissolution, census, sweep]
---

- The nav's movers know a class only by its name (`func_plat`, `func_train`, `func_door`). A pack's
  own (Dissolution's `func_new_plat`, Scourge's `func_train2`) is neither a floor nor a mover: the
  shaft it rides reads as open air. Before chasing one map's stall, count each pack's brush classes
  across every map (`brush ents <map> --root=<q1game> --game=<pack>`) for any the code never names:
  `func_new_plat` stood in 13 of Dissolution's 15 maps, 54 times.
- At run time the pack renames it (`plat`, the census's row), so the census's class table does not
  show the editor's name; read the map's entities for that.
- Get the mode's behaviour from a clean-room reader before modelling it (porting note
  q1-rogue-things.md): a named one waits for a use that moves nothing, so the bot stood on it for
  minutes when it was taken as a plat.
- Judge such a change on the pack's whole set of maps, not one: single maps move up and down by
  route changes alone (r2m4 35 to 27 kills on a door that has nothing to do with it). Compare on
  one binary built from the merged tree, the change switched by an environment variable: after a
  merge of main, r2m5 went from a finish to 13 kills, on a top-resting lift the first runs' routes
  had never met.
