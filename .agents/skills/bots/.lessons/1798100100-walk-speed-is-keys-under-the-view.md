---
title: A walk's speed past 320 is keys under the view; a give-up keyed by the exact link never fires
date: 2026-11-19
scope: plugins/shooterbots/pilot.cpp, plugins/shooterbots/pilot_jump.cpp, plugins/shooterbots/look_ahead.cpp, games/shooters_edge/bots/keyboard.cpp
concepts: [walkmash, mashstart, mashany, giveup, stairbump, walk-speed, ground-strafe, curve-keys, key-set, run-ups, kin, look-ahead]
---

- The ground tops the speed ALONG the wish up to 320; a wish 45 degrees off the run, swapped side
  to side every 0.1 s, settles near 395 (a short Python model of the ground move answered it in
  seconds; check a movement idea that way before building). Offered as a pilot's wish it failed:
  the keyboard turns the wish under the real view and cuts it to eight sets, and under a calm or
  aim view 45 degrees off a 45-degree wish became one strafe key (slower than W). Offer it as
  KEYS (`curve_keys`) and press them only while the view lies along the run.
- Read a walk's speed by stretches (ground 0.4 s+, positions over +-0.1 s): people walk at p50
  307-322 with 13-17 % at 360+; the trace's per-command speed and a mixed sample hid it.
- A give-up counted per exact (from, to) link never reached its limit: each plan again started
  the same jump from the node beside. Count by the jump's END and kind; cut its kin of its OWN
  kind (a rocket jump's too). Look for "lining up" loops in the trace by place before tuning.
- A look ahead that flies the box must not call a step it climbs a wall: a stair's riser costs a
  little speed in `player_move`, and a bump rule on speed lost let go of the keys on every climb.
