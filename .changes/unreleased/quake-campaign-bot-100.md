---
type: Added
title: A bot that plays Quake's campaign at 100%, and the se-q1-campaign lab
---

The way through (`games/shooters_edge/bots/way_through.hpp`) gains the 100% run: every place, button, trigger, secret and monster his routes reach before the exit, each tried twice before it is given up, with a time limit per map. Quake's key doors, linked doors, secret walls, waiting plats, lifts, teleporters out of reach, air tubes (`trigger_push`) and bridges that rise out of a pit are read into his plan. The `se-q1-campaign` lab plays each episode (or each map on its own with `--each=1`) and draws every map's route on the live map's plate, kills, secrets and the exit marked.
