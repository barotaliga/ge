---
title: A real map stalls a bot at every place its nav graph cannot see; find each with the lab's last-state notes
date: 2026-11-01
scope: games/shooters_edge/bots/fighter.cpp, games/shooters_edge/net/classic_team.cpp, plugins/shooter/swim.cpp
concepts: [team-game, reach, pocket, water, swim-field, dive, button, start-open-door, teleport, lost]
---

On the fixture halls a team game worked at once; on the shareware map the same bots stood idle
half the match. Every stall was a place the nav graph does not describe, and each one only
showed once the lab noted every player's last state (where, role, goal node, pilot mode) and,
for an idle one, whether `nav_nearest` found a node and how many nodes its reach held:

- an item the graph routes to but a door shuts: read the map's buttons (a `func_button` whose
  target is the door's name; one with `health` is shot, not touched) and pick the button's node
  by a clear line to the button, or the node behind the wall it is set in wins;
- a goal no route leads to: filter every goal by the fighter's own reach, worked out once a
  second, or the pilot replans in vain every command;
- a teleporter: add its links (`nav_add_links`), or half the map is a dead end;
- water: the proofs never swim, so close pools to entry (`nav_close_entries`) and give the bot a
  breadth-first field over the wet cells to the dry nodes (`swim.hpp`), seeded only from a ledge
  a swimmer can climb (80 units over the cell), or the field ends under a wall of the pool;
- an item in a pocket only the water reaches: a second field into that pocket, entered by one
  of the closed links into the water from a node that reaches most of the map again (not a pit);
- a door that starts open (spawnflags 1): it stands as a floor (a plate over a pit), so it is a
  solid of the nav's world, never a mover, or the bots stand on it off the graph.

Small pockets show in the reach count (a quarter of the map or less); `make lab LAB=se-team
ARGS="--basedir=... --items=all"` notes which items no route leads to from the main part.
