---
title: A fighter knows its own pickups off its status bar; a selector with nothing worth having picks nothing
date: 2026-11-11
scope: games/shooters_edge/bots/knowledge.cpp, games/shooters_edge/bots/fighter.cpp, engine/ai/choose.hpp, games/shooters_edge/labs/se_bots_lab.cpp, tests/shooters_edge_fight_test.cpp
concepts: [pickups, own-pickup, status-bar, item-knowledge, think, keys, se-bots, trace, took, pick-best, item-goal, idle, water]
---

- A red `pickups >= 1` in `shooters_edge_fight_test` (novice bot4, seed 2, 0 under keys) was not
  a movement bug: the server said it took 5 items. The fighter counted its own pickup only when
  it saw the item go from within `kOwnPickup` (64 units) at a think (every 0.1 s): at a keyboard
  run the next think after the touch was 70-80 units on, so the item was "found gone", untimed,
  and its goal held 0.2 s more. The experts lost half their pickups the same way.
- The fix is a person's cue, the status bar, and it took three rounds against the server's count
  (`FightPlayer::took`, now held within two by the test): a bare gain matched dead players' packs
  dropped by items (fights happen there), so a box must give ITS amount (or reach the cap), a
  weapon its own bit; the ammunition icon bits (256..2048) move with the weapon held and are no
  gain; and the view still sends a taken item for a frame or two, which re-confirmed it and
  counted it again (`ItemSpot::mine_at`, 0.3 s). The bar is reset at death: a respawn is no pickup.
- `ai::pick_best` handed back option 0 when every option scored 0, and the item goal scores what
  it cannot have 0: every idle bot (full health, nothing due) ran for the map's FIRST item, on
  e1m2 a health box across the water, two bots 50-60 % of a match. `pick_best`'s `above` (0 for
  the goals) returns -1 instead. Look for one goal node held by several bots for most of a match
  (`cut -f2,10 trace | sort | uniq -c`) before reading a route.
- Read a bot's belief against the server's truth before chasing a behaviour: `se-bots` reports
  `fight.<bot>.took` beside `pickups`, and `--trace=<tsv>` writes a row per fighter (mode, goal
  and its distance, pilot, axes, key set, feathering, pickups) through `FightOptions::watch`.
- A per-seed floor on a novice's frags against experts is a coin toss (about 4 a match: one of
  twelve novice matches at 0 about one run in six); the test holds the novices per seed as a
  pair, each player over the six seeds.
- Under keys a novice's move axes pass two hands: the pilot's stick thumb (0.18 s late, a
  spring) and then the keyboard's press and release. Passing the axes straight to the keyboard
  cut the wall bumps but slowed the bots (192 -> 184) and left them stuck longer: the thumb's
  smoothing keeps the feathering off a wish that dips at every corner. Judge a hands change on
  speed first. se-team's trace has `k`, `next_x`, `next_y`, `wish_f`, `wish_s`, `key_set`: a
  bump reads off them in one awk.
