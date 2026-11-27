---
title: No brink once in the pit, and a zero command is found by its last writer
date: 2026-11-26
scope: plugins/shooterbots/keyboard.cpp, tests/shooters_edge_footwork_test.cpp
concepts: [brinkkeys, keys-off-brink, slime, lava, swim, keyboard, keys-by, e1m4]
---

- `keys_off_brink` lets go of a key whose step ends over lava or slime. A bot already afloat in
  slime has harm under every step, so all four keys went and it floated still for ten minutes on
  e1m4, the swim field's way out pressed and dropped each command. The guard now stands down when
  the feet are in lava or slime: there is nothing left to keep off.
- `nav_harm_near` only sees harm nodes more than a step BELOW the point: a test of a player in a
  pit must float him over its floor, or the guard never runs and the test passes either way.
- To find what zeroed a command, print it right after the stage you trust and again at the end;
  `f.keys_by` names only the last stage marked with `keys_mark`, and the keyboard (`fighter_keys`
  / `fighter_keyboard`) rewrites the axes after every mark.
