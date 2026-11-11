---
title: A fighter knows its own pickups off its status bar; a selector with nothing worth having picks nothing
date: 2026-11-11
scope: games/shooters_edge/bots/knowledge.cpp, engine/ai/choose.hpp, games/shooters_edge/labs/se_bots_lab.cpp, tests/shooters_edge_fight_test.cpp
concepts: [pickups, own-pickup, status-bar, item-knowledge, keys, se-bots, trace, took, pick-best, item-goal, idle]
---

- A red `pickups >= 1` in the fight test was no movement bug: the server saw the bot take 5.
  It counted its own pickup only when it saw the item go from within 64 units at a think (every
  0.1 s); at a keyboard run the next think is 70-80 units on. Read a bot's belief against the
  server's truth first: `se-bots` reports `fight.<bot>.took` beside `pickups`, and `--trace`.
- The status bar is the person's cue, and it took three rounds against the server's count: a
  bare gain matched dead players' packs dropped by items, so a box must give ITS amount (or reach
  the cap), a weapon its own bit; the ammunition icon bits (256..2048) follow the weapon held;
  the view still sends a taken item a frame or two (`ItemSpot::mine_at`); the bar resets at death.
- `ai::pick_best` handed back option 0 when every option scored 0, and the item goal scores what
  it cannot have 0: every idle bot ran for the map's first item (e1m2: two bots 50-60 % of a
  match). `above` 0 returns -1. Look for one goal node held by several bots for most of a match
  (`cut -f2,10 trace | sort | uniq -c`) before reading a route.
- A per-seed floor on a novice's frags against experts is a coin toss (about 4 a match); hold the
  novices as a pair per seed and each player over the six seeds.
