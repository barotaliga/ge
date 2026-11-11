---
title: A walker that bumps corners must fly its own box through the move, not sweep a line; put each bump down to what took the speed first
date: 2026-11-10
scope: plugins/shooter/pilot.cpp, plugins/shooter/pilot.hpp, games/shooters_edge/labs/se_team_lab.cpp, games/shooters_edge/labs/why_log.hpp
concepts: [bumps, walk, look-ahead, foresee, player-move, corner, pursuit, momentum, lean, brake, side-key, bump-cause, se-team, cost]
---

- Before fixing bumps, name what took the speed (`team.bumps.by-*`, `by=` on the log's
  `speed_lost`): on dm2 a third of them were bots running into each other (`player`), and the
  ones with nothing ahead (`open`) were knockback from hits. Only `wall` is the walk's to fix.
- Most wall bumps are one-off corners clipped at 320 u/s, not loops. Pure pursuit turns its wish
  toward a point past the corner, which never takes the old direction's speed off (friction alone
  keeps it ~0.25 s), so the box carries into the corner's post however early the aim turns.
- What did NOT work, each six dm2 seeds: a lean off the wall a straight sweep meets (fires on
  ~19 % of walking commands, skimming corridor walls; speed 191 -> 172-178); the same lean only
  across the aim (no better); braking the turn by the side key on every turn (wall bumps up, speed
  174). A guess at how the body meets the room is wrong more often than right.
- What worked (`ahead`): FLY THE KEYS THROUGH `player_move` over the map's solids, the hands'
  delay first with the keys already pressed (`p.pressed`), then each candidate way for
  `ahead_s`, and press the way that does not bump, nearest the aim. Wall bumps 12.7 -> 10.3,
  speed up, not down, on six 3-minute seeds. The user's rule: a bot must know how its collision
  model will hit the environment. A one-sided 12-seed head to head first read -6 frags a match;
  read side by side over 24 seeds it was even (+0.4), and it went on in the presets (see the
  side-swap lesson, 1797002000).
- `--features=all,+x` is not "the default plus x": `all` also turns on what each skill's preset
  keeps off (`eye`, the novice's `glance`). Write `--features=+x`.
- Cost: a cheap straight sweep gates it, and the pursuit is flown first; the other ways only when
  it bumps: +5 % on a six-seed set (16.7 s -> 17.6 s). Time a set with ONE variant a call when
  comparing cost: three variants in one call share the cores (71 s) and read as a slowdown.
