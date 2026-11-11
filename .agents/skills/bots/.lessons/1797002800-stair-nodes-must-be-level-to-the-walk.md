---
title: A stair's nodes are level to the walk; read the pilot's arrival rule when a bot circles a node
date: 2026-11-11
scope: plugins/shooter/pilot.cpp, plugins/shooter/features.cpp, games/shooters_edge/bots/keyboard.cpp, games/shooters_edge/labs/se_team_lab.cpp
concepts: [slopes, stairs, level, pursue, passed, keys, hands, counter, feathering, trace, feature-names, codesign]
---

- `level()` decides where the pursuit looks past a node and whether a node is passed or must be
  REACHED. With a step's 18 units it took e1m2's stairs (20 between nodes 32 apart) for climbs:
  bots aimed at each stair node and circled it at a turn. `slopes` (a walk link to 45 degrees is
  level): +3.6 frags head to head, stuck 253 -> 194. When the trace's `k` stays put while the bot
  circles, read the pilot's arrival rule before its steering.
- Under keys a novice's move axes pass two hands (the pilot's stick thumb, 0.18 s and a spring,
  then the keyboard). Passing them straight to the keyboard, or pressing the keys against a slide
  across the wish (`counter`), cut the bumps and slowed the bots (192 -> 184, 194 -> 185): the
  thumb's smoothing and coasting keep speed. Judge a hands change on speed first.
- se-team's trace has `k`, `next_x`, `next_y`, `wish_f`, `wish_s`, `key_set`: a bump reads off
  them in one awk.
- `kNames` is sized by `kFeatures`, so a name left out compiles and crashes: `features.cpp`
  static_asserts the last name. The tag reader scans 60 lines: `features.hpp`'s list is at the
  edge. macOS kills (exit 137) a binary copied over one it ran: copy each A/B binary anew.
