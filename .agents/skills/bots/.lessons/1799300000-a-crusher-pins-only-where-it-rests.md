---
title: A crusher pins only where it rests, and a closet is a trap only while its door is shut
date: 2026-11-26
scope: plugins/shooterbots/map_nav.cpp, plugins/shooterbots/way_through.cpp, tests/shooterbots_map_nav_test.cpp, tests/shooterbots_way_through_test.cpp
concepts: [crusher, train, dmg, end-stop, harm, shut-trap, closet, barred-door, god-mode, campaign, e1m4, e1m3]
---

- Campaign bots cannot die, so a crushing train does not kill them: it holds them. On e1m4 the
  crusher two shot buttons bring down stays on its pit's floor (its last corner waits -1) and held
  the bot there for the rest of the map. `mark_crusher_harm` marks only the floor under the stop a
  train stays at for good; the top of it there is the way on and stays free.
- Marking the whole sweep of every `dmg` train closed off two things that matter: e1m4's crusher
  top (the bot's goal) and e1m3's trap-room ceiling, which comes down and goes back up (e1m3 23 to
  13 kills). Read a train's corners before deciding it is a hazard; most `dmg` trains only pass.
- `nav.trap` is computed once with every door open; a closet whose only way back is a door opened
  from elsewhere (e1m4's ogres, door t72 opened by a trigger in the pit) is a trap only while that
  door is barred. `way_shut_trap` recomputes it whenever the barred set changes.
- Fixing a stall uncovers the next one further along: e1m4's seeds 1 and 4 lost their exits
  because the bot now reaches the slime pit's water under the closet and finds no route out of it.
