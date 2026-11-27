---
title: Gear on arrival rides the program's own level change, never hand-written parms
date: 2026-11-27
scope: games/shooters_edge/bots/arrival_gear.cpp, games/shooters_edge/bots/coop_episode.cpp
concepts: [arrival-gear, spawn-parms, set-change-parms, touch, mission-pack, q2-campaign-order]
---

- To start a Quake map with gear, do not write `parm1..parm16` by hand: a mission pack keeps its
  own layout (Dissolution of Eternity's true ammunition sits in other fields, `OptionalFields::stock`).
  Start a new game on the earlier map, call each pickup's own `touch` with the player as `other`
  (`sh::call_program` with `g.self` the item), set the ammunition fields, then
  `campaign_change(change_level)`: the program's SetChangeParms and DecodeLevelParms carry it.
- Pick the pickups by `QcServer::spawned_as` (the map's class name), not the VM's classname: a pack's
  program renames some things after their spawn.
- Quake II carries its keys across levels in single player: carry them in the gear, less the ones an earlier map's `trigger_key` takes (`Q2Entity::needs`); Ground Zero's rboss opens only for the bomb brought to it.
- Quake II's loaders fill `MapSpec::brushes`, not `MapSpec::entities`: read a map's level changes
  off `brushes->entities` first. The unit lab's loader keeps every map it reads for the pictures, so
  scratch reads (the gear's earlier maps, the campaign order) go through a loader that keeps none.
