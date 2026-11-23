---
title: A party of campaign bots splits the work by holder each plan, and a single bot must not move a bit
date: 2026-11-23
scope: plugins/shooterbots/party.cpp, plugins/shooterbots/way_through.cpp, games/shooters_edge/bots/coop_episode.cpp, games/shooters_edge/bots/q2_unit.cpp
concepts: [party, co-op, coop, mates, split, holder, leash, partners, determinism]
---

- Write who holds each stop into every bot's plan input (`mates_steps`, `mates_prey`) before it
  plans; never let one bot read another's tour. The holder is the bot already on the stop, else
  the nearest, kept within `kPartyKeep` so two bots level with a stop do not trade it each plan.
- "Help a mate" must be decided from what the LAST plan found (own work left or not), or the bot
  flips between skipping and helping on alternate plans: count own and held stops separately and
  set `mates_help` only when no own stop is left.
- `left_to_do = left_to_do || open_step(st)` short-circuits the counting inside `open_step`: call
  it first.
- Quake II co-op leaves keys on the map for every player: count a key by any bot holding it, not
  by the key gone.
- Give partner k the seed plus a constant times k, so partner 0 keeps the single bot's seed and
  every golden and digest stays put.
