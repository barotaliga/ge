---
title: A fighter knows items off the map, its ears and its status bar, and every goal needs hysteresis, an end and its cause counted
date: 2026-10-30
scope: games/shooters_edge/profiles/items.hpp, games/shooters_edge/bots/knowledge.cpp, games/shooters_edge/bots/ears.cpp, games/shooters_edge/bots/packs.cpp, games/shooters_edge/bots/fighter.cpp, plugins/shooter/hearing.cpp, plugins/shooter/temper.cpp, plugins/shooter/traits.cpp, engine/ai/choose.hpp
concepts: [items, item-row, item-knowledge, hearing, pickup-sound, status-bar, own-pickup, packs, pick-best, hysteresis, retreat, temperament, traits, presets, lying, short-goal]
---

- Items, call words and sounds are rows of `profiles::kQ1Tables` (a variant before its plain
  row): edit a row, never the bots. Items come off the map's entity text, as a client has them (copy
  the names out); a taken item stops being sent, which times it fairly.
- Only a UNIQUE pickup sound says which item went; a shared one places the player only.
- Own pickups come off the status bar (a box gives ITS amount, a weapon its bit). Read a belief
  against the server's truth first (`se-bots` `took`); score a guess (`pack_guess`) where the truth
  is (`packs_truth`, in the runner), never in the bot.
- `ai::pick_best` returns -1 when nothing scores above 0; when it returned option 0, every idle bot
  ran for the map's first item. Look for one goal node held by many bots.
- Goals: hold the current one's score up by half; a retreat takes only health back within 3 s and
  its way out caps at 100; let go of a goal held 30 s in vain (8 s weak); a scarcity weight is the
  team's. Weigh a fight by might and skill (`self_edge`).
- An item left lying: break its time down by cause (`team.lying.<item>`): the YA was "on the way"
  two thirds of it, a missing node, not a missing want. Measure people's want first (packs: people
  take 99 %, the bots already 100 %). A taker (`fetch`) failed on its trips: split a trip by place
  and mode first. Short goals on the way: health, armour, a missing weapon, never ammunition or
  while backing off (frags 10.6 -> 12.1).
